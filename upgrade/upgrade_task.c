#include "upgrade_task.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "tx_api.h"
#include "tx_config.h"

#include "../hal/usb/diagnostic_tracking/hal_dt.h"
#include "rotcc.h"
#include "rotcc_protocol.h"
#include "upgrade.h"


#define TAG "upgrade_task: "
#define UPGRADE_START_PAYLOAD_SIZE (8U)
#define UPGRADE_REBOOT_FLUSH_TIMEOUT_MS (2000U)
#define UPGRADE_JOB_BLOCK_SIZE \
    ((sizeof(rot_upgrade_job_t) + sizeof(ULONG) - 1U) & ~(sizeof(ULONG) - 1U))
#define UPGRADE_JOB_POOL_SIZE \
    (UPGRADE_TASK_QUEUE_DEPTH * (UPGRADE_JOB_BLOCK_SIZE + sizeof(void *)))


_Static_assert(ROTCC_FW_MAX_PACKET_SIZE == INTF_CC_EP_SIZE,
               "CC protocol packet must match the CC endpoint size");
_Static_assert((UPGRADE_JOB_POOL_SIZE /
                (UPGRADE_JOB_BLOCK_SIZE + sizeof(void *))) >=
               UPGRADE_TASK_QUEUE_DEPTH,
               "upgrade job pool must hold every queue entry");
typedef enum {
    WORKER_STATE_IDLE = 0,
    WORKER_STATE_RECEIVING,
    WORKER_STATE_VERIFIED,
} worker_state_t;


static TX_QUEUE s_upgrade_queue;
static TX_BLOCK_POOL s_upgrade_job_pool;
static TX_THREAD s_upgrade_thread;
static ULONG s_upgrade_queue_storage[UPGRADE_TASK_QUEUE_DEPTH];
static ULONG s_upgrade_job_pool_storage[
    (UPGRADE_JOB_POOL_SIZE + sizeof(ULONG) - 1U) / sizeof(ULONG)];
static uint8_t s_upgrade_thread_stack[UPGRADE_TASK_STACK]
    __attribute__((aligned(8U)));

static bool s_initialized;
static rot_state_t s_init_result = rot_state_upgrade_not_init;
static UINT s_init_error;
static worker_state_t s_worker_state;
static uint32_t s_session_id;
static uint32_t s_file_size;
static uint32_t s_received_size;
static uint16_t s_next_data_seq;


static rot_state_t _send_ack(uint16_t seq)
{
    return rotcc_protocol_send_status(ROTCC_FW_TYPE_ACK, seq,
                                      rot_state_success);
}


static rot_state_t _send_nak(uint16_t seq, rot_state_t state)
{
    return rotcc_protocol_send_status(ROTCC_FW_TYPE_NAK, seq, state);
}


static void _put_le16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}


static void _handle_query(const rot_upgrade_job_t *job)
{
    upgrade_runtime_info_t info;
    uint8_t payload[24] = {0};
    rot_state_t state;

    state = upgrade_get_runtime_info(&info);
    if (state != rot_state_success) {
        (void)_send_nak(job->seq, state);
        return;
    }

    payload[0] = 3U;
    payload[1] = (uint8_t)(info.active_slot + 1U);
    payload[2] = info.slot_valid[0] ? 1U : 0U;
    payload[3] = info.slot_valid[1] ? 1U : 0U;
    _put_le16(&payload[4], info.active_version);
    _put_le16(&payload[6], info.active_boot_header_length);
    _put_le16(&payload[8], info.slot_version[0]);
    _put_le16(&payload[10], info.slot_version[1]);
    _put_le16(&payload[12], info.otp_sec_img_offset);
    _put_le16(&payload[14], info.otp_max_img_len);
    _put_le16(&payload[16], info.configured_slot_size_256k);
    payload[18] = (uint8_t)((info.remap_enabled ? 0x01U : 0U) |
                            (info.boot_ota_enabled ? 0x02U : 0U) |
                            (info.boot_layout_matches ? 0x04U : 0U));
    payload[20] = (uint8_t)info.firmware_version;
    payload[21] = (uint8_t)(info.firmware_version >> 8);
    payload[22] = (uint8_t)(info.firmware_version >> 16);
    payload[23] = (uint8_t)(info.firmware_version >> 24);

    state = rotcc_protocol_send_payload(ROTCC_FW_TYPE_INFO, job->seq,
                                        payload, sizeof(payload));
    if (state != rot_state_success) {
        hal_dt_printf(TAG "query response send failed: 0x%08x\r\n",
                      (unsigned)state);
    }
}


static void _reboot_after_response(uint8_t response_type, uint16_t seq, rot_state_t result, const char *reason)
{
    hal_dt_printf(TAG "%s, result=0x%08x, rebooting\r\n",
                  reason, (unsigned)result);
    (void)rotcc_protocol_send_status(response_type, seq, result);
    if (!rotcc_wait_idle(UPGRADE_REBOOT_FLUSH_TIMEOUT_MS)) {
        hal_dt_printf(TAG "response flush timeout before reboot\r\n");
    }
    upgrade_reboot();
}


static void _reset_session(bool abort_upgrade)
{
    if (abort_upgrade &&
        ((s_worker_state == WORKER_STATE_RECEIVING) ||
         (s_worker_state == WORKER_STATE_VERIFIED))) {
        (void)upgrade_abort();
    }

    s_worker_state = WORKER_STATE_IDLE;
    s_session_id = 0U;
    s_file_size = 0U;
    s_received_size = 0U;
    s_next_data_seq = 0U;
}


static void _handle_start(const rot_upgrade_job_t *job)
{
    upgrade_runtime_info_t info;
    rot_state_t state;

    if (job->payload_len != UPGRADE_START_PAYLOAD_SIZE) {
        (void)_send_nak(job->seq, rot_state_invalid_argument);
        return;
    }
    if ((job->file_size < sizeof(upgrade_image_header_t)) ||
        (job->file_size > (UPGRADE_IMAGE_MAX_SIZE +
                           sizeof(upgrade_image_header_t)))) {
        (void)_send_nak(job->seq, rot_state_invalid_argument);
        return;
    }

    state = upgrade_get_runtime_info(&info);
    if (state != rot_state_success) {
        (void)_send_nak(job->seq, state);
        return;
    }
    if (!info.boot_ota_enabled || !info.boot_layout_matches) {
        hal_dt_printf(TAG "boot layout mismatch: otp=%u, configured=%u\r\n",
                      (unsigned)info.otp_sec_img_offset,
                      (unsigned)info.configured_slot_size_256k);
        (void)_send_nak(job->seq, rot_state_upgrade_boot_layout_mismatch);
        return;
    }

    /* The previous START ACK may have been lost. Do not erase a second time. */
    if ((s_worker_state == WORKER_STATE_RECEIVING) &&
        (s_session_id == job->session_id) &&
        (s_file_size == job->file_size) &&
        (s_received_size == 0U) &&
        (s_next_data_seq == 0U)) {
        (void)_send_ack(job->seq);
        return;
    }

    _reset_session(true);

    state = upgrade_init();
    if (state != rot_state_success) {
        hal_dt_printf(TAG "upgrade init failed: 0x%08x\r\n",
                      (unsigned)state);
        _reboot_after_response(ROTCC_FW_TYPE_NAK, job->seq, state,
                               "upgrade init failed");
        return;
    }
    s_session_id = job->session_id;
    s_file_size = job->file_size;
    s_received_size = 0U;
    s_next_data_seq = 0U;
    s_worker_state = WORKER_STATE_RECEIVING;

    hal_dt_printf(TAG "start session=0x%08x, size=%u\r\n",
                  (unsigned)s_session_id, (unsigned)s_file_size);
    (void)_send_ack(job->seq);
}


static void _handle_data(const rot_upgrade_job_t *job)
{
    bool image_complete = false;
    rot_state_t state;

    if (s_worker_state != WORKER_STATE_RECEIVING) {
        (void)_send_nak(job->seq, rot_state_upgrade_incomplete);
        return;
    }

    /* ACK a retransmission whose previous response was lost. */
    if ((s_next_data_seq != 0U) &&
        (job->seq == (uint16_t)(s_next_data_seq - 1U))) {
        (void)_send_ack(job->seq);
        return;
    }
    if (job->seq != s_next_data_seq) {
        (void)_send_nak(job->seq, rot_state_upgrade_incomplete);
        return;
    }
    if (s_received_size > s_file_size) {
        (void)_send_nak(job->seq, rot_state_upgrade_incomplete);
        return;
    }
    if ((job->payload_len == 0U) ||
        (job->payload_len > ROTCC_FW_MAX_PAYLOAD_SIZE) ||
        (job->payload_len > (s_file_size - s_received_size))) {
        (void)_send_nak(job->seq, rot_state_invalid_argument);
        return;
    }

    state = upgrade_feed(job->payload, job->payload_len, &image_complete);
    if (state != rot_state_success) {
        upgrade_status_t upgrade_status;
        bool fatal =
            (upgrade_get_status(&upgrade_status) == rot_state_success) &&
            (upgrade_status.state == UPGRADE_STATE_FAILED);

        hal_dt_printf(TAG "data seq=%u failed: 0x%08x\r\n",
                      (unsigned)job->seq, (unsigned)state);
        if (fatal) {
            _reboot_after_response(ROTCC_FW_TYPE_NAK, job->seq, state,
                                   "image write failed");
            return;
        } else {
            (void)_send_nak(job->seq, state);
            return;
        }
    }

    s_received_size += job->payload_len;
    s_next_data_seq++;
    if (((s_next_data_seq & 0x3fU) == 0U) || image_complete) {
        hal_dt_printf(TAG "rx %u/%u bytes%s\r\n",
                      (unsigned)s_received_size, (unsigned)s_file_size,
                      image_complete ? " (image complete)" : "");
    }
    (void)_send_ack(job->seq);
}


static void _handle_end(const rot_upgrade_job_t *job)
{
    rot_state_t state;

    if (s_worker_state == WORKER_STATE_VERIFIED) {
        (void)_send_ack(job->seq);
        return;
    }
    if (s_worker_state != WORKER_STATE_RECEIVING) {
        (void)_send_nak(job->seq, rot_state_upgrade_incomplete);
        return;
    }
    if (s_received_size != s_file_size) {
        _reboot_after_response(ROTCC_FW_TYPE_NAK, job->seq, rot_state_upgrade_incomplete, "image incomplete at END");
        return;
    }

    state = upgrade_finish();
    if (state != rot_state_success) {
        hal_dt_printf(TAG "finish failed: 0x%08x\r\n", (unsigned)state);
        _reboot_after_response(ROTCC_FW_TYPE_NAK, job->seq, state, "image verification failed");
        return;
    }

    s_worker_state = WORKER_STATE_VERIFIED;
    hal_dt_printf(TAG "verified %u bytes\r\n", (unsigned)s_received_size);
    _reboot_after_response(ROTCC_FW_TYPE_ACK, job->seq, rot_state_success, "upgrade completed");
}


static void _handle_abort(const rot_upgrade_job_t *job)
{
    _reset_session(true);
    hal_dt_printf(TAG "aborted by host\r\n");
    _reboot_after_response(ROTCC_FW_TYPE_ACK, job->seq, rot_state_success,
                           "upgrade aborted");
}


static void _process_job(const rot_upgrade_job_t *job)
{
    switch ((rot_upgrade_command_t)job->command) {
    case ROT_UPGRADE_CMD_START:
        _handle_start(job);
        break;
    case ROT_UPGRADE_CMD_DATA:
        _handle_data(job);
        break;
    case ROT_UPGRADE_CMD_END:
        _handle_end(job);
        break;
    case ROT_UPGRADE_CMD_ABORT:
        _handle_abort(job);
        break;
    case ROT_UPGRADE_CMD_QUERY:
        _handle_query(job);
        break;
    default:
        (void)_send_nak(job->seq, rot_state_not_supported);
        break;
    }
}


static void _upgrade_thread_entry(ULONG argument)
{
    rot_upgrade_job_t *job = NULL;

    (void)argument;

    for (;;) {
        if (TX_SUCCESS == tx_queue_receive(&s_upgrade_queue, &job, TX_WAIT_FOREVER)) {
            _process_job(job);
            (void)tx_block_release(job);
            job = NULL;
        }
    }
}


rot_state_t upgrade_task_init(void)
{
    UINT ret;

    if (s_initialized) {
        return rot_state_success;
    }

    ret = tx_queue_create(&s_upgrade_queue,
                          "upgrade_queue",
                          1U,
                          s_upgrade_queue_storage,
                          sizeof(s_upgrade_queue_storage));
    if (ret != TX_SUCCESS) {
        printf(TAG "queue create failed, ret=%u\r\n", (unsigned)ret);
        s_init_error = ret;
        s_init_result = rot_state_upgrade_init_fail;
        return rot_state_upgrade_init_fail;
    }

    ret = tx_block_pool_create(&s_upgrade_job_pool,
                               "upgrade_jobs",
                               UPGRADE_JOB_BLOCK_SIZE,
                               s_upgrade_job_pool_storage,
                               sizeof(s_upgrade_job_pool_storage));
    if (ret != TX_SUCCESS) {
        (void)tx_queue_delete(&s_upgrade_queue);
        printf(TAG "job pool create failed, ret=%u\r\n", (unsigned)ret);
        s_init_error = ret;
        s_init_result = rot_state_upgrade_init_fail;
        return rot_state_upgrade_init_fail;
    }

    ret = tx_thread_create(&s_upgrade_thread, "thread_upgrade", _upgrade_thread_entry, 0U, s_upgrade_thread_stack, sizeof(s_upgrade_thread_stack), UPGRADE_TASK_PRIORITY, UPGRADE_TASK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ret != TX_SUCCESS) {
        (void)tx_block_pool_delete(&s_upgrade_job_pool);
        (void)tx_queue_delete(&s_upgrade_queue);
        printf(TAG "thread create failed, ret=%u\r\n", (unsigned)ret);
        s_init_error = ret;
        s_init_result = rot_state_upgrade_init_fail;
        return rot_state_upgrade_init_fail;
    }

    s_worker_state = WORKER_STATE_IDLE;
    s_initialized = true;
    s_init_result = rot_state_success;
    s_init_error = TX_SUCCESS;
    return rot_state_success;
}


rot_state_t upgrade_task_submit(const rot_upgrade_job_t *job)
{
    rot_upgrade_job_t *block = NULL;
    UINT ret;

    if (!s_initialized) {
        return s_init_result;
    }
    if (job == NULL) {
        return rot_state_invalid_argument;
    }

    ret = tx_block_allocate(&s_upgrade_job_pool, (VOID **)&block, TX_NO_WAIT);
    if (ret != TX_SUCCESS) {
        return rot_state_upgrade_busy;
    }

    memcpy(block, job, sizeof(*job));
    ret = tx_queue_send(&s_upgrade_queue, &block, TX_NO_WAIT);
    if (ret != TX_SUCCESS) {
        (void)tx_block_release(block);
        return (ret == TX_QUEUE_FULL)
            ? rot_state_upgrade_busy : rot_state_fail;
    }
    return rot_state_success;
}


bool upgrade_task_is_running(void)
{
    return s_initialized;
}


uint32_t upgrade_task_get_init_error(void)
{
    return (uint32_t)s_init_error;
}
