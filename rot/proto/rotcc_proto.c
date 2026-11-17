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
static uint32_t s_fw_size;
static uint32_t s_fw_crc32;
static uint32_t s_received_size;
static bool s_initialized;

static uint32_t s_keymap[KBD_MAP_MAX][KBD_KEY_MAX];
static uint8_t s_active_layer;
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

static void _handle_sys_handshake(const rotcc_msg_t *msg)
{
    uint8_t version[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    _put_le32(version, ROTCC_PROTO_VERSION);
    (void)_reply(msg, version, sizeof(version));
}

static void _handle_sys_key_count(const rotcc_msg_t *msg)
{
    uint8_t count[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    _put_le32(count, KBD_KEY_MAX);
    (void)_reply(msg, count, sizeof(count));
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
        s_active_layer = 0U;
        state = fdb_update(FDB_ITEM_TY_MAP, FDB_SAVE_TY_NOW);
    }
    /* RGB / advanced-matrix modules are not present on this device. */
    if ((options & (ROTCC_RESET_RGB | ROTCC_RESET_ADV_MT)) != 0U) {
        hal_dt_printf(TAG "RGB/ADV reset ignored (module absent)\r\n");
    }
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

static void _handle_kc_layer_count(const rotcc_msg_t *msg)
{
    uint8_t count[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    _put_le32(count, KBD_MAP_MAX);
    (void)_reply(msg, count, sizeof(count));
}

static void _handle_kc_active_layer(const rotcc_msg_t *msg)
{
    uint8_t layer[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    _put_le32(layer, s_active_layer);
    (void)_reply(msg, layer, sizeof(layer));
}

static void _handle_kc_set_active_layer(const rotcc_msg_t *msg)
{
    uint32_t layer;

    if (msg->payload_len != sizeof(uint32_t)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    layer = _get_le32(msg->payload);
    if (layer >= KBD_MAP_MAX) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    s_active_layer = (uint8_t)layer;
    _reply_status(msg, rotsta_success);
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
    s_fw_size = 0U;
    s_fw_crc32 = 0U;
    s_received_size = 0U;
}

static void _handle_upgrade_info(const rotcc_msg_t *msg)
{
    upgrade_info_t info;
    uint8_t payload[ROTCC_UPGRADE_INFO_LEN] = {0};
    rotsta_t state;

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    state = upgrade_get_info(&info);
    if (state != rotsta_success) {
        _reply_status(msg, state);
        return;
    }

    payload[0] = info.active_slot;
    payload[1] = info.slot_count;
    _put_le32(&payload[2], info.slot_size);
    payload[6] = info.encrypted;

    (void)_reply(msg, payload, sizeof(payload));
}

static void _handle_upgrade_start(const rotcc_msg_t *msg, uint32_t fw_size, uint32_t fw_crc32)
{
    rotsta_t state;

    /* The previous START response may have been lost. Do not erase twice. */
    if ((s_worker_state == WORKER_STATE_RECEIVING) &&
        (s_fw_size == fw_size) &&
        (s_fw_crc32 == fw_crc32) &&
        (s_received_size == 0U)) {
        _reply_status(msg, rotsta_success);
        return;
    }

    _upgrade_reset_session(true);

    state = upgrade_init();
    if (state != rotsta_success) {
        hal_dt_printf(TAG "upgrade init failed: 0x%08x\r\n", (unsigned)state);
        _reply_status(msg, state);
        return;
    }

    state = upgrade_start(fw_size, fw_crc32);
    if (state != rotsta_success) {
        hal_dt_printf(TAG "upgrade start failed: 0x%08x\r\n", (unsigned)state);
        _upgrade_reset_session(true);
        _reply_status(msg, state);
        return;
    }

    s_fw_size = fw_size;
    s_fw_crc32 = fw_crc32;
    s_received_size = 0U;
    s_worker_state = WORKER_STATE_RECEIVING;

    hal_dt_printf(TAG "upgrade start, size=%u, crc=0x%08x\r\n",
                  (unsigned)s_fw_size, (unsigned)s_fw_crc32);
    _reply_status(msg, rotsta_success);
}

static void _handle_upgrade_finish(const rotcc_msg_t *msg, uint32_t fw_size, uint32_t fw_crc32)
{
    rotsta_t state;

    if (s_worker_state == WORKER_STATE_VERIFIED) {
        _reply_status(msg, rotsta_success);
        return;
    }
    if (s_worker_state != WORKER_STATE_RECEIVING) {
        _reply_status(msg, rotsta_upgrade_incomplete);
        return;
    }
    if ((s_received_size != s_fw_size) ||
        (fw_size != s_fw_size) ||
        (fw_crc32 != s_fw_crc32)) {
        _upgrade_reset_session(true);
        _reply_status(msg, rotsta_upgrade_incomplete);
        return;
    }

    state = upgrade_finish();
    if (state != rotsta_success) {
        hal_dt_printf(TAG "finish failed: 0x%08x\r\n", (unsigned)state);
        _upgrade_reset_session(true);
        _reply_status(msg, state);
        return;
    }

    s_worker_state = WORKER_STATE_VERIFIED;
    hal_dt_printf(TAG "verified %u bytes\r\n", (unsigned)s_received_size);
    _reply_and_reboot(msg, rotsta_success, "upgrade completed");
}

static void _handle_upgrade_request(const rotcc_msg_t *msg)
{
    uint32_t req_ty;
    uint32_t fw_size;
    uint32_t fw_crc32;

    if (msg->payload_len != ROTCC_UPGRADE_REQUEST_LEN) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    req_ty = _get_le32(&msg->payload[0]);
    fw_size = _get_le32(&msg->payload[4]);
    fw_crc32 = _get_le32(&msg->payload[8]);

    switch (req_ty) {
    case ROTCC_UPGRADE_REQ_START:
        _handle_upgrade_start(msg, fw_size, fw_crc32);
        break;
    case ROTCC_UPGRADE_REQ_STOP:
        _upgrade_reset_session(true);
        hal_dt_printf(TAG "upgrade stopped by host\r\n");
        _reply_status(msg, rotsta_success);
        break;
    case ROTCC_UPGRADE_REQ_FINISH:
        _handle_upgrade_finish(msg, fw_size, fw_crc32);
        break;
    default:
        _reply_status(msg, rotsta_invalid_argument);
        break;
    }
}

static void _handle_upgrade_data(const rotcc_msg_t *msg)
{
    rotsta_t state;

    if (msg->payload_len == 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    if (s_worker_state != WORKER_STATE_RECEIVING) {
        _reply_status(msg, rotsta_upgrade_incomplete);
        return;
    }
    if ((s_received_size >= s_fw_size) ||
        (msg->payload_len > (s_fw_size - s_received_size))) {
        _upgrade_reset_session(true);
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    state = upgrade_write(msg->payload, msg->payload_len);
    if (state != rotsta_success) {
        hal_dt_printf(TAG "data failed at %u: 0x%08x\r\n",
                      (unsigned)s_received_size, (unsigned)state);
        _upgrade_reset_session(true);
        _reply_status(msg, state);
        return;
    }

    s_received_size += msg->payload_len;
    if ((s_received_size & 0x3FFFU) < msg->payload_len) {
        hal_dt_printf(TAG "rx %u/%u\r\n",
                      (unsigned)s_received_size, (unsigned)s_fw_size);
    }
    _reply_status(msg, rotsta_success);
}

static void _handle_upgrade(const rotcc_msg_t *msg)
{
    switch (msg->subcmd) {
    case ROTCC_UPGRADE_INFO:
        _handle_upgrade_info(msg);
        break;
    case ROTCC_UPGRADE_REQUEST:
        _handle_upgrade_request(msg);
        break;
    case ROTCC_UPGRADE_DATA:
        _handle_upgrade_data(msg);
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
        case ROTCC_SYS_HANDSHAKE: _handle_sys_handshake(msg); break;
        case ROTCC_SYS_INFO:      _handle_sys_info(msg);      break;
        case ROTCC_SYS_REBOOT:    _handle_sys_reboot(msg);    break;
        case ROTCC_SYS_RESET:     _handle_sys_reset(msg);     break;
        case ROTCC_SYS_KEY_COUNT: _handle_sys_key_count(msg); break;
        default:                  _reply_status(msg, rotsta_not_supported); break;
        }
        break;
    case ROTCC_CMD_KC:
        switch (msg->subcmd) {
        case ROTCC_KC_GET:              _handle_kc_get(msg);              break;
        case ROTCC_KC_SET:              _handle_kc_set(msg);              break;
        case ROTCC_KC_LAYER_COUNT:      _handle_kc_layer_count(msg);      break;
        case ROTCC_KC_ACTIVE_LAYER:     _handle_kc_active_layer(msg);     break;
        case ROTCC_KC_SET_ACTIVE_LAYER: _handle_kc_set_active_layer(msg); break;
        default:                        _reply_status(msg, rotsta_not_supported); break;
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
    s_active_layer = 0U;
    _keymap_init();
    rotcc_register_frame_cb(_on_frame);
    s_initialized = true;
    return rotsta_success;
}
