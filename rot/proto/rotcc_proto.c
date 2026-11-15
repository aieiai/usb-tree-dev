#include "rotcc_proto.h"

#include <stddef.h>
#include <string.h>

#include "fdb.h"
#include "fdb_map.h"
#include "kbd_config.h"
#include "rotcc.h"
#include "rotver.h"
#include "upgrade.h"
#include "usb_config.h"
#include "usb/diagnostic_tracking/hal_dt.h"


#define TAG "rotcc_proto: "
#define ROTCC_UPGRADE_START_LEN     (8U)
#define ROTCC_UPGRADE_SEQ_LEN       (2U)
#define ROTCC_UPGRADE_INFO_LEN      (28U)
#define ROTCC_UPGRADE_INFO_VERSION  (4U)
#define ROTCC_REBOOT_FLUSH_TIMEOUT_MS (2000U)


typedef struct __attribute__((packed)) {
    uint16_t vid;
    uint16_t pid;
    uint8_t pn[16];
    uint8_t sn[16];
    uint64_t fw_ver;
    uint8_t hw_ver;
} rotcc_device_info_t;

_Static_assert(sizeof(rotcc_device_info_t) == 45U, "device info payload layout changed");


typedef enum {
    WORKER_STATE_IDLE = 0,
    WORKER_STATE_RECEIVING,
    WORKER_STATE_VERIFIED,
} worker_state_t;


static worker_state_t s_worker_state;
static uint32_t s_session_id;
static uint32_t s_file_size;
static uint32_t s_received_size;
static uint16_t s_next_data_seq;
static bool s_initialized;

static uint32_t s_keymap[KBD_MAP_MAX][KBD_KEY_MAX];
static uint8_t s_tx_payload[ROTGEN_MAX_PAYLOAD_SIZE] __attribute__((aligned(4)));


static uint16_t _get_le16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t _get_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void _put_le16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void _put_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}


static rotsta_t _reply(const rotcc_msg_t *msg, const void *payload, uint16_t payload_len)
{
    uint16_t total = (uint16_t)(ROTCC_MSG_HEADER_SIZE + payload_len);

    if (total > sizeof(s_tx_payload)) {
        return rotsta_invalid_argument;
    }
    if ((payload == NULL) && (payload_len != 0U)) {
        return rotsta_invalid_argument;
    }

    s_tx_payload[0] = msg->cmd;
    s_tx_payload[1] = msg->subcmd;
    _put_le16(&s_tx_payload[2], msg->reserve);
    if (payload_len != 0U) {
        memcpy(&s_tx_payload[ROTCC_MSG_HEADER_SIZE], payload, payload_len);
    }
    return rotcc_send_frame(0U, s_tx_payload, total);
}

static void _reply_status(const rotcc_msg_t *msg, rotsta_t state)
{
    uint8_t status[sizeof(uint32_t)];

    _put_le32(status, state);
    (void)_reply(msg, status, sizeof(status));
}

/* Sends the final response, waits until the endpoint took it, then resets. */
static void _reply_and_reboot(const rotcc_msg_t *msg, rotsta_t state, const char *reason)
{
    hal_dt_printf(TAG "%s, result=0x%08x, rebooting\r\n", reason, (unsigned)state);
    _reply_status(msg, state);
    if (!rotcc_wait_idle(ROTCC_REBOOT_FLUSH_TIMEOUT_MS)) {
        hal_dt_printf(TAG "response flush timeout before reboot\r\n");
    }
    upgrade_reboot();
}


/* ------------------------------------------------------------------ system */

static void _handle_sys_version(const rotcc_msg_t *msg)
{
    uint8_t version[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    _put_le32(version, ROTCC_PROTO_VERSION);
    (void)_reply(msg, version, sizeof(version));
}

static void _handle_sys_handshake(const rotcc_msg_t *msg)
{
    _reply_status(msg, rotsta_success);
}

static void _handle_sys_info(const rotcc_msg_t *msg)
{
    rotcc_device_info_t info;

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    memset(&info, 0, sizeof(info));
    info.vid = USBD_VID;
    info.pid = USBD_PID;
    info.fw_ver = (uint64_t)FW_VER;
    if (g_sobi.magic == FDB_SOBI_MAGIC) {
        info.vid = g_sobi.pi.vid;
        info.pid = g_sobi.pi.pid;
        memcpy(info.pn, g_sobi.pi.pn, sizeof(info.pn));
        memcpy(info.sn, g_sobi.pi.sn, sizeof(info.sn));
        info.fw_ver = g_sobi.pi.fv;
        info.hw_ver = (uint8_t)ROTVER_GET_MAJOR(g_sobi.pi.hv);
    }
    (void)_reply(msg, &info, (uint16_t)sizeof(info));
}

static void _handle_sys_reboot(const rotcc_msg_t *msg)
{
    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    _reply_and_reboot(msg, rotsta_success, "reboot requested");
}

static void _handle_sys_reset(const rotcc_msg_t *msg)
{
    uint32_t options;
    rotsta_t state = rotsta_success;

    if (msg->payload_len != sizeof(uint32_t)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    options = _get_le32(msg->payload);
    if ((options & ROTCC_RESET_KC) != 0U) {
        memcpy(s_keymap, keymap, sizeof(s_keymap));
        state = fdb_update(FDB_ITEM_TY_MAP, FDB_SAVE_TY_D5S);
    }
    /* RGB / advanced-matrix modules are not present on this device. */
    if (state != rotsta_success) {
        hal_dt_printf(TAG "reset failed: 0x%08x\r\n", (unsigned)state);
    }
    _reply_status(msg, state);
}


/* ------------------------------------------------------------------ keymap */

static void _keymap_init(void)
{
    bool blank = true;
    rotsta_t state;

    state = fdb_register(FDB_ITEM_TY_MAP, s_keymap, (uint32_t)sizeof(s_keymap));
    if (state != rotsta_success) {
        hal_dt_printf(TAG "keymap load failed: 0x%08x, using defaults\r\n", (unsigned)state);
        memcpy(s_keymap, keymap, sizeof(s_keymap));
        return;
    }

    for (uint32_t layer = 0U; layer < KBD_MAP_MAX; layer++) {
        for (uint32_t key = 0U; key < KBD_KEY_MAX; key++) {
            if (s_keymap[layer][key] != FDB_INVALID_DAT) {
                blank = false;
                break;
            }
        }
    }
    if (blank) {
        memcpy(s_keymap, keymap, sizeof(s_keymap));
        (void)fdb_update(FDB_ITEM_TY_MAP, FDB_SAVE_TY_D5S);
    }
}

/* reserve: low byte key index, high byte layer. */
static bool _kc_index(const rotcc_msg_t *msg, uint8_t *layer, uint8_t *key)
{
    uint8_t index = (uint8_t)(msg->reserve & 0xFFU);

    *layer = (uint8_t)(msg->reserve >> 8);
    *key = index;
    return (index < KBD_KEY_MAX) && (*layer < KBD_MAP_MAX);
}

static void _handle_kc_get(const rotcc_msg_t *msg)
{
    uint8_t layer;
    uint8_t key;
    uint8_t code[sizeof(uint32_t)];

    if ((msg->payload_len != 0U) || !_kc_index(msg, &layer, &key)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    _put_le32(code, s_keymap[layer][key]);
    (void)_reply(msg, code, sizeof(code));
}

static void _handle_kc_set(const rotcc_msg_t *msg)
{
    uint8_t layer;
    uint8_t key;
    rotsta_t state;

    if ((msg->payload_len != sizeof(uint32_t)) || !_kc_index(msg, &layer, &key)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    s_keymap[layer][key] = _get_le32(msg->payload);
    state = fdb_update(FDB_ITEM_TY_MAP, FDB_SAVE_TY_D5S);
    _reply_status(msg, state);
}


/* ----------------------------------------------------------------- upgrade */

static void _upgrade_reset_session(bool abort_upgrade)
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

static void _handle_upgrade_query(const rotcc_msg_t *msg)
{
    upgrade_runtime_info_t info;
    uint8_t payload[ROTCC_UPGRADE_INFO_LEN] = {0};
    rotsta_t state;

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    state = upgrade_get_runtime_info(&info);
    if (state != rotsta_success) {
        _reply_status(msg, state);
        return;
    }

    payload[0] = ROTCC_UPGRADE_INFO_VERSION;
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
                            (info.boot_layout_matches ? 0x04U : 0U) |
                            (info.slot_retired[0] ? 0x08U : 0U) |
                            (info.slot_retired[1] ? 0x10U : 0U) |
                            (info.slot_record_valid[0] ? 0x20U : 0U) |
                            (info.slot_record_valid[1] ? 0x40U : 0U) |
                            (info.otp_exip_enabled ? 0x80U : 0U));
    _put_le32(&payload[20], info.firmware_version);
    _put_le16(&payload[24], info.slot_app_version[0]);
    _put_le16(&payload[26], info.slot_app_version[1]);

    (void)_reply(msg, payload, sizeof(payload));
}

static void _handle_upgrade_start(const rotcc_msg_t *msg)
{
    upgrade_runtime_info_t info;
    uint32_t file_size;
    uint32_t session_id;
    rotsta_t state;

    if (msg->payload_len != ROTCC_UPGRADE_START_LEN) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    file_size = _get_le32(&msg->payload[0]);
    session_id = _get_le32(&msg->payload[4]);
    if ((file_size < sizeof(upgrade_image_header_t)) ||
        (file_size > (UPGRADE_IMAGE_MAX_SIZE + sizeof(upgrade_image_header_t)))) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    state = upgrade_get_runtime_info(&info);
    if (state != rotsta_success) {
        _reply_status(msg, state);
        return;
    }
    if (!info.boot_ota_enabled || !info.boot_layout_matches) {
        hal_dt_printf(TAG "boot layout mismatch: otp=%u, configured=%u\r\n",
                      (unsigned)info.otp_sec_img_offset,
                      (unsigned)info.configured_slot_size_256k);
        _reply_status(msg, rotsta_upgrade_boot_layout_mismatch);
        return;
    }

    /* The previous START response may have been lost. Do not erase twice. */
    if ((s_worker_state == WORKER_STATE_RECEIVING) &&
        (s_session_id == session_id) &&
        (s_file_size == file_size) &&
        (s_received_size == 0U) &&
        (s_next_data_seq == 0U)) {
        _reply_status(msg, rotsta_success);
        return;
    }

    _upgrade_reset_session(true);

    state = upgrade_init();
    if (state != rotsta_success) {
        hal_dt_printf(TAG "upgrade init failed: 0x%08x\r\n", (unsigned)state);
        _reply_and_reboot(msg, state, "upgrade init failed");
        return;
    }

    s_session_id = session_id;
    s_file_size = file_size;
    s_received_size = 0U;
    s_next_data_seq = 0U;
    s_worker_state = WORKER_STATE_RECEIVING;

    hal_dt_printf(TAG "start session=0x%08x, size=%u\r\n",
                  (unsigned)s_session_id, (unsigned)s_file_size);
    _reply_status(msg, rotsta_success);
}

static void _handle_upgrade_data(const rotcc_msg_t *msg)
{
    const uint8_t *image;
    uint32_t image_len;
    uint16_t seq;
    bool image_complete = false;
    rotsta_t state;

    if (msg->payload_len <= ROTCC_UPGRADE_SEQ_LEN) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    if (s_worker_state != WORKER_STATE_RECEIVING) {
        _reply_status(msg, rotsta_upgrade_incomplete);
        return;
    }

    seq = _get_le16(&msg->payload[0]);
    image = &msg->payload[ROTCC_UPGRADE_SEQ_LEN];
    image_len = (uint32_t)(msg->payload_len - ROTCC_UPGRADE_SEQ_LEN);

    /* Answer a retransmission whose previous response was lost. */
    if ((s_next_data_seq != 0U) && (seq == (uint16_t)(s_next_data_seq - 1U))) {
        _reply_status(msg, rotsta_success);
        return;
    }
    if (seq != s_next_data_seq) {
        _reply_status(msg, rotsta_upgrade_incomplete);
        return;
    }
    if ((s_received_size > s_file_size) ||
        (image_len > (s_file_size - s_received_size))) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    state = upgrade_feed(image, image_len, &image_complete);
    if (state != rotsta_success) {
        upgrade_status_t upgrade_status;
        bool fatal =
            (upgrade_get_status(&upgrade_status) == rotsta_success) &&
            (upgrade_status.state == UPGRADE_STATE_FAILED);

        hal_dt_printf(TAG "data seq=%u failed: 0x%08x\r\n",
                      (unsigned)seq, (unsigned)state);
        if (fatal && (state != rotsta_upgrade_exip_image_required)) {
            _reply_and_reboot(msg, state, "image write failed");
            return;
        }
        _reply_status(msg, state);
        return;
    }

    s_received_size += image_len;
    s_next_data_seq++;
    if (((s_next_data_seq & 0x3fU) == 0U) || image_complete) {
        hal_dt_printf(TAG "rx %u/%u bytes%s\r\n",
                      (unsigned)s_received_size, (unsigned)s_file_size,
                      image_complete ? " (image complete)" : "");
    }
    _reply_status(msg, rotsta_success);
}

static void _handle_upgrade_end(const rotcc_msg_t *msg)
{
    rotsta_t state;

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    if (s_worker_state == WORKER_STATE_VERIFIED) {
        _reply_status(msg, rotsta_success);
        return;
    }
    if (s_worker_state != WORKER_STATE_RECEIVING) {
        _reply_status(msg, rotsta_upgrade_incomplete);
        return;
    }
    if (s_received_size != s_file_size) {
        _reply_and_reboot(msg, rotsta_upgrade_incomplete, "image incomplete at END");
        return;
    }

    state = upgrade_finish();
    if (state != rotsta_success) {
        hal_dt_printf(TAG "finish failed: 0x%08x\r\n", (unsigned)state);
        _reply_and_reboot(msg, state, "image verification failed");
        return;
    }

    s_worker_state = WORKER_STATE_VERIFIED;
    hal_dt_printf(TAG "verified %u bytes\r\n", (unsigned)s_received_size);
    _reply_and_reboot(msg, rotsta_success, "upgrade completed");
}

static void _handle_upgrade_abort(const rotcc_msg_t *msg)
{
    _upgrade_reset_session(true);
    hal_dt_printf(TAG "aborted by host\r\n");
    _reply_and_reboot(msg, rotsta_success, "upgrade aborted");
}

static void _handle_upgrade(const rotcc_msg_t *msg)
{
    switch (msg->subcmd) {
    case ROTCC_UPGRADE_QUERY:
        _handle_upgrade_query(msg);
        break;
    case ROTCC_UPGRADE_START:
        _handle_upgrade_start(msg);
        break;
    case ROTCC_UPGRADE_DATA:
        _handle_upgrade_data(msg);
        break;
    case ROTCC_UPGRADE_END:
        _handle_upgrade_end(msg);
        break;
    case ROTCC_UPGRADE_ABORT:
        _handle_upgrade_abort(msg);
        break;
    default:
        _reply_status(msg, rotsta_not_supported);
        break;
    }
}


/* --------------------------------------------------------------- dispatch */

static void _handle_msg(const rotcc_msg_t *msg)
{
    switch (msg->cmd) {
    case ROTCC_CMD_SYS:
        switch (msg->subcmd) {
        case ROTCC_SYS_VERSION:   _handle_sys_version(msg);   break;
        case ROTCC_SYS_HANDSHAKE: _handle_sys_handshake(msg); break;
        case ROTCC_SYS_INFO:      _handle_sys_info(msg);      break;
        case ROTCC_SYS_REBOOT:    _handle_sys_reboot(msg);    break;
        case ROTCC_SYS_RESET:     _handle_sys_reset(msg);     break;
        default:                  _reply_status(msg, rotsta_not_supported); break;
        }
        break;
    case ROTCC_CMD_KC:
        switch (msg->subcmd) {
        case ROTCC_KC_GET: _handle_kc_get(msg); break;
        case ROTCC_KC_SET: _handle_kc_set(msg); break;
        default:           _reply_status(msg, rotsta_not_supported); break;
        }
        break;
    case ROTCC_CMD_UPGRADE:
        _handle_upgrade(msg);
        break;
    default:
        _reply_status(msg, rotsta_not_supported);
        break;
    }
}

static void _on_frame(const rotgen_frame_t *frame, void *ctx)
{
    rotcc_msg_t msg;

    (void)ctx;

    if (((frame->options & ROTGEN_OPT_REQUEST) == 0U) ||
        (frame->payload_len < ROTCC_MSG_HEADER_SIZE)) {
        return;
    }

    msg.cmd = frame->payload[0];
    msg.subcmd = frame->payload[1];
    msg.reserve = _get_le16(&frame->payload[2]);
    msg.payload_len = (uint16_t)(frame->payload_len - ROTCC_MSG_HEADER_SIZE);
    msg.payload = &frame->payload[ROTCC_MSG_HEADER_SIZE];

    _handle_msg(&msg);
}

rotsta_t rotcc_proto_init(void)
{
    if (s_initialized) {
        return rotsta_success;
    }

    s_worker_state = WORKER_STATE_IDLE;
    _keymap_init();
    rotcc_register_frame_cb(_on_frame);
    s_initialized = true;
    return rotsta_success;
}
