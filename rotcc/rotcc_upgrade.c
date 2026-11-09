#include "rotcc_upgrade.h"

#include <string.h>

#include "../hal/usb/diagnostic_tracking/hal_dt.h"
#include "rotcc_protocol.h"
#include "upgrade_task.h"


#define TAG "ccfw: "
#define ROTCC_FW_START_PAYLOAD_SIZE (8U)


static rot_state_t _nak(uint16_t seq, rot_state_t state)
{
    return rotcc_protocol_send_status(ROTCC_FW_TYPE_NAK, seq, state);
}


static void _init_job(rot_upgrade_job_t *job, rot_upgrade_command_t command,
                      uint16_t seq)
{
    memset(job, 0, sizeof(*job));
    job->command = (uint8_t)command;
    job->seq = seq;
}


static rot_state_t _submit(rot_upgrade_job_t *job, uint16_t seq)
{
    rot_state_t state = upgrade_task_submit(job);

    if (state != rot_state_success) {
        hal_dt_printf(TAG "queue command failed: 0x%08x, init_error=%u\r\n",
                      (unsigned)state,
                      (unsigned)upgrade_task_get_init_error());
        (void)_nak(seq, state);
    }
    return state;
}


static void _handle_start(const rotcc_protocol_frame_t *frame)
{
    rot_upgrade_job_t job;

    if (frame->payload_len != ROTCC_FW_START_PAYLOAD_SIZE) {
        (void)_nak(frame->seq, rot_state_invalid_argument);
        return;
    }

    _init_job(&job, ROT_UPGRADE_CMD_START, frame->seq);
    job.session_id = rotcc_protocol_get_le32(&frame->payload[4]);
    job.file_size = rotcc_protocol_get_le32(&frame->payload[0]);
    job.payload_len = frame->payload_len;
    memcpy(job.payload, frame->payload, frame->payload_len);
    (void)_submit(&job, frame->seq);
}


static void _handle_data(const rotcc_protocol_frame_t *frame)
{
    rot_upgrade_job_t job;

    if (frame->payload_len == 0U) {
        (void)_nak(frame->seq, rot_state_invalid_argument);
        return;
    }

    _init_job(&job, ROT_UPGRADE_CMD_DATA, frame->seq);
    job.payload_len = frame->payload_len;
    memcpy(job.payload, frame->payload, frame->payload_len);
    (void)_submit(&job, frame->seq);
}


static void _handle_simple(const rotcc_protocol_frame_t *frame,
                           rot_upgrade_command_t command)
{
    rot_upgrade_job_t job;

    if (frame->payload_len != 0U) {
        (void)_nak(frame->seq, rot_state_invalid_argument);
        return;
    }

    _init_job(&job, command, frame->seq);
    (void)_submit(&job, frame->seq);
}


void rotcc_upgrade_on_rx(const uint8_t *data, uint16_t len)
{
    rotcc_protocol_frame_t frame;
    rot_state_t state;

    state = rotcc_protocol_parse(data, len, &frame);
    if (state == rot_state_not_found) {
        return;
    }
    if (state != rot_state_success) {
        uint16_t seq = ((data != NULL) && (len >= 6U))
            ? (uint16_t)((uint16_t)data[4] | ((uint16_t)data[5] << 8))
            : 0U;
        (void)_nak(seq, state);
        return;
    }

    switch (frame.type) {
    case ROTCC_FW_TYPE_START:
        _handle_start(&frame);
        break;
    case ROTCC_FW_TYPE_DATA:
        _handle_data(&frame);
        break;
    case ROTCC_FW_TYPE_END:
        _handle_simple(&frame, ROT_UPGRADE_CMD_END);
        break;
    case ROTCC_FW_TYPE_ABORT:
        _handle_simple(&frame, ROT_UPGRADE_CMD_ABORT);
        break;
    case ROTCC_FW_TYPE_QUERY:
        _handle_simple(&frame, ROT_UPGRADE_CMD_QUERY);
        break;
    default:
        (void)_nak(frame.seq, rot_state_not_supported);
        break;
    }
}


void rot_cc_on_rx_packet(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    rotcc_upgrade_on_rx(data, len);
}
