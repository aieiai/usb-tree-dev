#include "rotcc_proto.h"

#include <stddef.h>
#include <string.h>

#include "db.h"

#include "kbd_config.h"
#include "keyprop.h"
#include "keymap.h"
#include "keycalib.h"
#include "keymagnet.h"
#include "keyevt.h"


#include "rotcc.h"
#include "rotver.h"
#include "upgrade.h"
#include "usb_config.h"
#include "hal_usb.h"


#define TAG "rotcc_proto: "
#define ROTCC_REBOOT_FLUSH_TIMEOUT_MS   (2000U)
#define ROTCC_EXT_CMD_MAX               (8U)

typedef struct {
    uint8_t cmd;
    uint8_t subcmd;
    rotcc_cmd_handler_t handler;
} rotcc_cmd_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t vid;
    uint16_t pid;
    uint8_t pn[16];
    uint8_t sn[16];
    uint64_t fw_ver;
    uint8_t hw_ver;
    uint8_t cd[16];     /* build time, YYYYMMDDHHMMSS */
} rotcc_device_info_t;

_Static_assert(sizeof(rotcc_device_info_t) == 61U, "device info payload layout changed");
_Static_assert(sizeof(upgrade_info_t) == ROTCC_UPGRADE_INFO_LEN, "upgrade info payload layout changed");
_Static_assert(sizeof(rotkc_t) == ROTCC_KC_CODE_LEN, "key code payload layout changed");


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

static uint8_t s_tx_payload[ROTGEN_MAX_PAYLOAD_SIZE] __attribute__((aligned(4)));
/* One entry per key in the requested range; the tick entry is the largest. */
static uint8_t s_key_payload[ROTCC_KC_TICK_LEN * KBD_KEY_MAX] __attribute__((aligned(4)));

_Static_assert(sizeof(s_key_payload) <= ROTGEN_MAX_PAYLOAD_SIZE, "per-key reply exceeds one frame");



rotsta_t rotcc_proto_reply(const rotcc_msg_t *msg, const void *payload, uint16_t payload_len)
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
    rotutil_put_le16(&s_tx_payload[2], msg->reserve);
    if (payload_len != 0U) {
        memcpy(&s_tx_payload[ROTCC_MSG_HEADER_SIZE], payload, payload_len);
    }
    return rotcc_send_frame(0U, s_tx_payload, total);
}

static rotsta_t _reply_status(const rotcc_msg_t *msg, rotsta_t state)
{
    uint8_t status[sizeof(uint32_t)];
    rotutil_put_le32(status, state);
    return rotcc_proto_reply(msg, status, sizeof(status));
}

// Sends the final response, waits until the endpoint took it, then resets.
static void _reply_and_reboot(const rotcc_msg_t *msg, rotsta_t state, const char *reason)
{
    hal_dt_printf(TAG "%s, result=0x%08x, rebooting\r\n", reason, (unsigned)state);
    _reply_status(msg, state);
    if (!rotcc_wait_idle(ROTCC_REBOOT_FLUSH_TIMEOUT_MS)) {
        hal_dt_printf(TAG "response flush timeout before reboot\r\n");
    }
    upgrade_restart();
}


static void _handle_sys_handshake(const rotcc_msg_t *msg)
{
    uint8_t version[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    rotutil_put_le32(version, ROTCC_PROTO_VERSION);
    (void)rotcc_proto_reply(msg, version, sizeof(version));
}

static void _handle_sys_key_count(const rotcc_msg_t *msg)
{
    uint8_t count[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    rotutil_put_le32(count, KBD_KEY_MAX);
    (void)rotcc_proto_reply(msg, count, sizeof(count));
}

static void _handle_sys_popup_support(const rotcc_msg_t *msg)
{
    uint8_t supported[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    rotutil_put_le32(supported, (HAL_USB_WEBUSB_ENABLE != 0) ? 1U : 0U);
    (void)rotcc_proto_reply(msg, supported, sizeof(supported));
}

static void _handle_sys_popup_get(const rotcc_msg_t *msg)
{
    uint8_t enabled[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    
    rotutil_put_le32(enabled, (HAL_USB_WEBUSB_ENABLE != 0) ? (uint32_t)g_obi.fs.cc_popup : 0U);
    (void)rotcc_proto_reply(msg, enabled, sizeof(enabled));
}

static void _handle_sys_popup_set(const rotcc_msg_t *msg)
{
    uint8_t prev;
    uint32_t value;
    rotsta_t state;

    if (msg->payload_len != sizeof(uint32_t)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    if (HAL_USB_WEBUSB_ENABLE == 0) {
        _reply_status(msg, rotsta_not_supported);
        return;
    }

    value = rotutil_get_le32(msg->payload);
    if (value > 1U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    if (g_obi.fs.cc_popup == value) {
        _reply_status(msg, rotsta_success);
        return;
    }

    prev = g_obi.fs.cc_popup;
    g_obi.fs.cc_popup = value ? 1U : 0U;
    state = db_update(DB_ITEM_OBI, DB_SAVE_TY_NOW);
    if (state != rotsta_success) {
        g_obi.fs.cc_popup = prev;
        hal_dt_printf(TAG "popup save failed: 0x%08x\r\n", (unsigned)state);
    }
    _reply_status(msg, state);
}

static void _handle_sys_info(const rotcc_msg_t *msg)
{
    rotcc_device_info_t info;

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    memset(&info, 0, sizeof(info));
    memcpy(info.cd, rotver_get_cd(), sizeof(rotver_get_cd()) - 1);
    info.vid = USBD_VID;
    info.pid = USBD_PID;
    info.fw_ver = (uint64_t)FW_VER;
    if (g_obi.magic == MAGIC_OBI) {
        info.vid = g_obi.pi.vid;
        info.pid = g_obi.pi.pid;
        memcpy(info.pn, g_obi.pi.pn, sizeof(info.pn));
        memcpy(info.sn, g_obi.pi.sn, sizeof(info.sn));
        memcpy(info.cd, g_obi.pi.cd, sizeof(info.cd));
        info.fw_ver = g_obi.pi.fv;
        info.hw_ver = (uint8_t)ROTVER_GET_MAJOR(g_obi.pi.hv);
    }
    (void)rotcc_proto_reply(msg, &info, (uint16_t)sizeof(info));
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

    options = rotutil_get_le32(msg->payload);
    if ((options & ROTCC_RESET_KC) != 0U) {
        state = keymap_reset();
        if (state == rotsta_success) {
            state = db_update(DB_ITEM_OBM, DB_SAVE_TY_NOW);
        }
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

static void _upgrade_reset_session(bool abort_upgrade)
{
    if (abort_upgrade &&
        ((s_worker_state == WORKER_STATE_RECEIVING) || (s_worker_state == WORKER_STATE_VERIFIED))) {
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

    (void)rotcc_proto_reply(msg, &info, (uint16_t)sizeof(info));
}

static void _handle_upgrade_start(const rotcc_msg_t *msg, uint32_t fw_size, uint32_t fw_crc32)
{
    rotsta_t state;

    // The previous START response may have been lost. Do not erase twice.
    if ((s_worker_state == WORKER_STATE_RECEIVING) && (s_fw_size == fw_size) && (s_fw_crc32 == fw_crc32) && (s_received_size == 0U)) {
        _reply_status(msg, rotsta_success);
        return;
    }

    _upgrade_reset_session(true);

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

    hal_dt_printf(TAG "upgrade start, size=%u, crc=0x%08x\r\n", (unsigned)s_fw_size, (unsigned)s_fw_crc32);
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
    if ((s_received_size != s_fw_size) || (fw_size != s_fw_size) || (fw_crc32 != s_fw_crc32)) {
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

    req_ty = rotutil_get_le32(&msg->payload[0]);
    fw_size = rotutil_get_le32(&msg->payload[4]);
    fw_crc32 = rotutil_get_le32(&msg->payload[8]);

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
    if ((s_received_size >= s_fw_size) || (msg->payload_len > (s_fw_size - s_received_size))) {
        _upgrade_reset_session(true);
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    state = upgrade_write(msg->payload, msg->payload_len);
    if (state != rotsta_success) {
        hal_dt_printf(TAG "data failed at %u: 0x%08x\r\n", (unsigned)s_received_size, (unsigned)state);
        _upgrade_reset_session(true);
        _reply_status(msg, state);
        return;
    }

    s_received_size += msg->payload_len;
    if ((s_received_size & 0x3FFFU) < msg->payload_len) {
        hal_dt_printf(TAG "rx %u/%u\r\n", (unsigned)s_received_size, (unsigned)s_fw_size);
    }
    _reply_status(msg, rotsta_success);
}

/* Query payload is {start, end, rsv, rsv}; every key in the range gets its own reply entry. */
static rotsta_t _key_query(const rotcc_msg_t *msg, uint8_t *start, uint8_t *end, uint32_t *count)
{
    if (msg->payload_len != ROTCC_KC_QUERY_LEN) {
        return rotsta_invalid_argument;
    }

    *start = msg->payload[0];
    *end = msg->payload[1];
    if ((*end < *start) || (*end >= KBD_KEY_MAX)) {
        return rotsta_invalid_argument;
    }

    *count = (uint32_t)(*end - *start) + 1U;
    return rotsta_success;
}

/* Set payload is {start, end, entry}; the one entry is applied to the whole range. */
static rotsta_t _key_set_range(const rotcc_msg_t *msg, uint16_t entry_len, uint8_t *start, uint8_t *end, uint32_t *count)
{
    if (msg->payload_len != (uint16_t)(ROTCC_KC_RANGE_HEADER_LEN + entry_len)) {
        return rotsta_invalid_argument;
    }

    *start = msg->payload[0];
    *end = msg->payload[1];
    if ((*end < *start) || (*end >= KBD_KEY_MAX)) {
        return rotsta_invalid_argument;
    }

    *count = (uint32_t)(*end - *start) + 1U;
    return rotsta_success;
}

static void _handle_kc_get(const rotcc_msg_t *msg)
{
    uint8_t layer = (uint8_t)(msg->reserve >> 8);
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t offset = 0U;

    if ((layer >= KBD_MAP_MAX) || (_key_query(msg, &start, &end, &count) != rotsta_success)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        rotutil_put_le32(&s_key_payload[offset], keymap_get_key(layer, (uint8_t)(start + i)));
        offset += ROTCC_KC_CODE_LEN;
    }
    (void)rotcc_proto_reply(msg, s_key_payload, (uint16_t)offset);
}

static void _handle_kc_set(const rotcc_msg_t *msg)
{
    uint8_t layer = (uint8_t)(msg->reserve >> 8);
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t code;
    rotsta_t state = rotsta_success;

    if ((layer >= KBD_MAP_MAX) || (_key_set_range(msg, ROTCC_KC_CODE_LEN, &start, &end, &count) != rotsta_success)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    code = rotutil_get_le32(&msg->payload[ROTCC_KC_RANGE_HEADER_LEN]);
    for (uint32_t i = 0U; i < count; i++) {
        state = keymap_set_key(layer, (uint8_t)(start + i), code);
        if (state != rotsta_success) {
            break;
        }
    }

    if (state == rotsta_success) {
        state = db_update(DB_ITEM_OBM, DB_SAVE_TY_D5S);
    }
    _reply_status(msg, state);
}

static void _handle_kc_layer_count(const rotcc_msg_t *msg)
{
    uint8_t count[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    rotutil_put_le32(count, KBD_MAP_MAX);
    (void)rotcc_proto_reply(msg, count, sizeof(count));
}

static void _handle_kc_active_layer(const rotcc_msg_t *msg)
{
    uint8_t layer[sizeof(uint32_t)];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    rotutil_put_le32(layer, keymap_get_active_layer());
    (void)rotcc_proto_reply(msg, layer, sizeof(layer));
}

static void _handle_kc_set_active_layer(const rotcc_msg_t *msg)
{
    uint32_t layer;
    rotsta_t state;

    if (msg->payload_len != sizeof(uint32_t)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    layer = rotutil_get_le32(msg->payload);
    state = keymap_set_active_layer((uint8_t)layer);
    if (state == rotsta_success) {
        state = db_update(DB_ITEM_OBM, DB_SAVE_TY_D5S);
    }
    _reply_status(msg, state);
}

static void _handle_kc_tick_get(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t offset = 0U;

    if (_key_query(msg, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        const keyattr_t *p_attr = &g_obkp.attr[start + i];

        rotutil_put_le16(&s_key_payload[offset + 0U], p_attr->act.sclt);
        rotutil_put_le16(&s_key_payload[offset + 2U], p_attr->act.sclto);
        rotutil_put_le16(&s_key_payload[offset + 4U], p_attr->act.dclt);
        rotutil_put_le16(&s_key_payload[offset + 6U], p_attr->act.dclto);
        offset += ROTCC_KC_TICK_LEN;
    }
    (void)rotcc_proto_reply(msg, s_key_payload, (uint16_t)offset);
}

static void _handle_kc_tick_set(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    rotsta_t state;

    if (_key_set_range(msg, ROTCC_KC_TICK_LEN, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        keyattr_t *p_attr = &g_obkp.attr[start + i];
        const uint8_t *entry = &msg->payload[ROTCC_KC_RANGE_HEADER_LEN];

        p_attr->act.sclt = rotutil_get_le16(&entry[0U]);
        p_attr->act.sclto = rotutil_get_le16(&entry[2U]);
        p_attr->act.dclt = rotutil_get_le16(&entry[4U]);
        p_attr->act.dclto = rotutil_get_le16(&entry[6U]);
    }

    state = db_update(DB_ITEM_OBKP, DB_SAVE_TY_D5S);
    _reply_status(msg, state);
}

static void _handle_kc_trg_get(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t offset = 0U;

    if (_key_query(msg, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        rotutil_put_le32(&s_key_payload[offset], (uint32_t)g_obkp.attr[start + i].trg_ty);
        offset += ROTCC_KC_TRG_LEN;
    }
    (void)rotcc_proto_reply(msg, s_key_payload, (uint16_t)offset);
}

static void _handle_kc_trg_set(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t trg_ty;
    rotsta_t state;

    if (_key_set_range(msg, ROTCC_KC_TRG_LEN, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    trg_ty = rotutil_get_le32(&msg->payload[ROTCC_KC_RANGE_HEADER_LEN]);
    if (trg_ty > (uint32_t)KTRG_TY_RT_CTN) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        g_obkp.attr[start + i].trg_ty = (ktrg_ty)trg_ty;
    }

    state = db_update(DB_ITEM_OBKP, DB_SAVE_TY_D5S);
    _reply_status(msg, state);
}

static void _handle_kc_dz_get(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t offset = 0U;

    if (_key_query(msg, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        const keyattr_t *p_attr = &g_obkp.attr[start + i];

        rotutil_put_le16(&s_key_payload[offset], p_attr->dz.dst_t);
        rotutil_put_le16(&s_key_payload[offset + 2U], p_attr->dz.dst_b);
        offset += ROTCC_KC_DZ_LEN;
    }
    (void)rotcc_proto_reply(msg, s_key_payload, (uint16_t)offset);
}

static void _handle_kc_dz_set(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    rotsta_t state;

    if (_key_set_range(msg, ROTCC_KC_DZ_LEN, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        keyattr_t *p_attr = &g_obkp.attr[start + i];
        const uint8_t *entry = &msg->payload[ROTCC_KC_RANGE_HEADER_LEN];

        p_attr->dz.dst_t = rotutil_get_le16(&entry[0U]);
        p_attr->dz.dst_b = rotutil_get_le16(&entry[2U]);
    }

    state = db_update(DB_ITEM_OBKP, DB_SAVE_TY_D5S);
    _reply_status(msg, state);
}

static void _handle_kc_nom_get(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t offset = 0U;

    if (_key_query(msg, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        const keyattr_t *p_attr = &g_obkp.attr[start + i];

        rotutil_put_le16(&s_key_payload[offset], p_attr->nom.dst_p);
        rotutil_put_le16(&s_key_payload[offset + 2U], p_attr->nom.dst_r);
        offset += ROTCC_KC_NOM_LEN;
    }
    (void)rotcc_proto_reply(msg, s_key_payload, (uint16_t)offset);
}

static void _handle_kc_nom_set(const rotcc_msg_t *msg)
{
    uint8_t start, end, is_valid = 0;
    uint16_t dst_p, dst_r;
    uint32_t count;
    rotsta_t state;

    if (_key_set_range(msg, ROTCC_KC_NOM_LEN, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        keyattr_t *p_attr = &g_obkp.attr[start + i];
        const uint8_t *entry = &msg->payload[ROTCC_KC_RANGE_HEADER_LEN];

        dst_p = rotutil_get_le16(&entry[0U]);
        dst_r = rotutil_get_le16(&entry[2U]);
        if ((dst_p != 0) && (dst_p != p_attr->nom.dst_p)) {
            is_valid = 1;
            p_attr->nom.dst_p = dst_p;
        }
        if ((dst_r != 0) && (dst_r != p_attr->nom.dst_r)) {
            is_valid = 1;
            p_attr->nom.dst_r = dst_r;
        }
    }

    if (!is_valid) {
        _reply_status(msg, rotsta_fail);
        return;
    }

    keyevt_reload();
    state = db_update(DB_ITEM_OBKP, DB_SAVE_TY_D5S);
    _reply_status(msg, state);
}

static void _handle_kc_rt_get(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t offset = 0U;

    if (_key_query(msg, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        const keyattr_t *p_attr = &g_obkp.attr[start + i];

        rotutil_put_le16(&s_key_payload[offset + 0U], p_attr->rt.dst_p);
        rotutil_put_le16(&s_key_payload[offset + 2U], p_attr->rt.dst_r);
        offset += ROTCC_KC_RT_LEN;
    }
    (void)rotcc_proto_reply(msg, s_key_payload, (uint16_t)offset);
}

static void _handle_kc_rt_set(const rotcc_msg_t *msg)
{
    uint8_t start, end, is_valid = 0;
    uint16_t dst_p, dst_r;
    uint32_t count;
    rotsta_t state;

    if (_key_set_range(msg, ROTCC_KC_RT_LEN, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        keyattr_t *p_attr = &g_obkp.attr[start + i];
        const uint8_t *entry = &msg->payload[ROTCC_KC_RANGE_HEADER_LEN];

        dst_p = rotutil_get_le16(&entry[0U]);
        dst_r = rotutil_get_le16(&entry[2U]);
        if ((dst_p != 0) && (dst_p != p_attr->rt.dst_p)) {
            is_valid = 1;
            p_attr->rt.dst_p = dst_p;
        }
        if ((dst_r != 0) && (dst_r != p_attr->rt.dst_r)) {
            is_valid = 1;
            p_attr->rt.dst_r = dst_r;
        }
    }

    if (!is_valid) {
        _reply_status(msg, rotsta_fail);
        return;
    }

    state = db_update(DB_ITEM_OBKP, DB_SAVE_TY_D5S);
    _reply_status(msg, state);
}

static bool _axis_supported(uint8_t ty)
{
    for (uint32_t i = 0U; (i < (uint32_t)KAXIS_MAX) && ((uint8_t)g_kbd_axis_support[i] != (uint8_t)KAXIS_MAX); i++) {
        if ((uint8_t)g_kbd_axis_support[i] == ty) {
            return true;
        }
    }
    return false;
}

static void _handle_axis_list(const rotcc_msg_t *msg)
{
    uint32_t count = 0U;

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    while ((count < (uint32_t)KAXIS_MAX) && ((uint8_t)g_kbd_axis_support[count] != (uint8_t)KAXIS_MAX)) {
        s_key_payload[count] = (uint8_t)g_kbd_axis_support[count];
        count++;
    }
    (void)rotcc_proto_reply(msg, s_key_payload, (uint16_t)count);
}

static void _handle_axis_get(const rotcc_msg_t *msg)
{
    uint8_t start;
    uint8_t end;
    uint32_t count;
    uint32_t offset = 0U;

    if (_key_query(msg, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        rotutil_put_le32(&s_key_payload[offset], (uint32_t)g_obkp.attr[start + i].axis.ty);
        offset += ROTCC_KC_AXIS_LEN;
    }
    (void)rotcc_proto_reply(msg, s_key_payload, (uint16_t)offset);
}

static void _handle_axis_set(const rotcc_msg_t *msg)
{
    uint8_t start, end, is_valid = 0;
    uint32_t count;
    uint32_t axis;
    rotsta_t state;

    if (_key_set_range(msg, ROTCC_KC_AXIS_LEN, &start, &end, &count) != rotsta_success) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    axis = rotutil_get_le32(&msg->payload[ROTCC_KC_RANGE_HEADER_LEN]);
    if ((axis >= (uint32_t)KAXIS_MAX) || !_axis_supported((uint8_t)axis)) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        uint8_t ki = (uint8_t)(start + i);
        if (g_obkp.attr[ki].axis.ty != (kaxis_ty)axis) {
            is_valid = 1;
            g_obkp.attr[ki].axis.ty = (kaxis_ty)axis;
            keymag_update_factor(ki);
        }
    }

    if (!is_valid) {
        _reply_status(msg, rotsta_fail);
        return;
    }

    keyevt_reload();
    state = db_update(DB_ITEM_OBKP, DB_SAVE_TY_D5S);
    _reply_status(msg, state);
}

static void _handle_calib_status_get(const rotcc_msg_t *msg)
{
    uint8_t payload[2];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    payload[0] = _keycal_get_status();
    payload[1] = 0U;
    (void)rotcc_proto_reply(msg, payload, sizeof(payload));
}

static void _handle_calib_status_set(const rotcc_msg_t *msg)
{
    rotsta_t state;

    if (msg->payload_len != 2U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    state = keycal_set_status(msg->payload[0]);
    hal_dt_printf(TAG "calib set 0x%02x, result=0x%08x\r\n", (unsigned)msg->payload[0], (unsigned)state);
    _reply_status(msg, state);
}

static void _handle_calib_auto_get(const rotcc_msg_t *msg)
{
    uint8_t payload[2];

    if (msg->payload_len != 0U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    payload[0] = (g_obi.fs.auto_calib != 0U) ? 1U : 0U;
    payload[1] = 0U;
    (void)rotcc_proto_reply(msg, payload, sizeof(payload));
}

static void _handle_calib_auto_set(const rotcc_msg_t *msg)
{
    uint8_t prev;
    uint32_t value;
    rotsta_t state;

    if (msg->payload_len != 2U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }

    value = msg->payload[0];
    if (value > 1U) {
        _reply_status(msg, rotsta_invalid_argument);
        return;
    }
    if ((uint32_t)g_obi.fs.auto_calib == value) {
        _reply_status(msg, rotsta_success);
        return;
    }

    prev = g_obi.fs.auto_calib;
    g_obi.fs.auto_calib = (uint8_t)value;
    state = db_update(DB_ITEM_OBI, DB_SAVE_TY_NOW);
    if (state != rotsta_success) {
        g_obi.fs.auto_calib = prev;
        hal_dt_printf(TAG "auto calib save failed: 0x%08x\r\n", (unsigned)state);
    }
    _reply_status(msg, state);
}

// External test hooks, only used when no built-in handler matches.
static rotcc_cmd_entry_t ext_cmd_table[ROTCC_EXT_CMD_MAX];
static const rotcc_cmd_entry_t cmd_table[] = {
    { ROTCC_CMD_SYS,     ROTCC_SYS_HANDSHAKE,       _handle_sys_handshake        },
    { ROTCC_CMD_SYS,     ROTCC_SYS_INFO,            _handle_sys_info             },
    { ROTCC_CMD_SYS,     ROTCC_SYS_REBOOT,          _handle_sys_reboot           },
    { ROTCC_CMD_SYS,     ROTCC_SYS_RESET,           _handle_sys_reset            },
    { ROTCC_CMD_SYS,     ROTCC_SYS_KEY_COUNT,       _handle_sys_key_count        },
    { ROTCC_CMD_SYS,     ROTCC_SYS_POPUP_SUPPORT,   _handle_sys_popup_support    },
    { ROTCC_CMD_SYS,     ROTCC_SYS_POPUP_GET,       _handle_sys_popup_get        },
    { ROTCC_CMD_SYS,     ROTCC_SYS_POPUP_SET,       _handle_sys_popup_set        },
    
    { ROTCC_CMD_KC,      ROTCC_KC_GET,              _handle_kc_get               },
    { ROTCC_CMD_KC,      ROTCC_KC_SET,              _handle_kc_set               },
    { ROTCC_CMD_KC,      ROTCC_KC_LAYER_COUNT,      _handle_kc_layer_count       },
    { ROTCC_CMD_KC,      ROTCC_KC_ACTIVE_LAYER,     _handle_kc_active_layer      },
    { ROTCC_CMD_KC,      ROTCC_KC_SET_ACTIVE_LAYER, _handle_kc_set_active_layer  },
    { ROTCC_CMD_KC,      ROTCC_KC_TICK_GET,         _handle_kc_tick_get          },
    { ROTCC_CMD_KC,      ROTCC_KC_TICK_SET,         _handle_kc_tick_set          },
    { ROTCC_CMD_KC,      ROTCC_KC_TRG_GET,          _handle_kc_trg_get           },
    { ROTCC_CMD_KC,      ROTCC_KC_TRG_SET,          _handle_kc_trg_set           },
    { ROTCC_CMD_KC,      ROTCC_KC_DZ_GET,           _handle_kc_dz_get            },
    { ROTCC_CMD_KC,      ROTCC_KC_DZ_SET,           _handle_kc_dz_set            },
    { ROTCC_CMD_KC,      ROTCC_KC_NOM_GET,          _handle_kc_nom_get           },
    { ROTCC_CMD_KC,      ROTCC_KC_NOM_SET,          _handle_kc_nom_set           },
    { ROTCC_CMD_KC,      ROTCC_KC_RT_GET,           _handle_kc_rt_get            },
    { ROTCC_CMD_KC,      ROTCC_KC_RT_SET,           _handle_kc_rt_set            },

    { ROTCC_CMD_AXIS,    ROTCC_AXIS_LIST,           _handle_axis_list            },
    { ROTCC_CMD_AXIS,    ROTCC_AXIS_GET,            _handle_axis_get             },
    { ROTCC_CMD_AXIS,    ROTCC_AXIS_SET,            _handle_axis_set             },

    { ROTCC_CMD_CALIB,   ROTCC_CALIB_STATUS_GET,    _handle_calib_status_get     },
    { ROTCC_CMD_CALIB,   ROTCC_CALIB_STATUS_SET,    _handle_calib_status_set     },
    { ROTCC_CMD_CALIB,   ROTCC_CALIB_AUTO_GET,      _handle_calib_auto_get       },
    { ROTCC_CMD_CALIB,   ROTCC_CALIB_AUTO_SET,      _handle_calib_auto_set       },

    { ROTCC_CMD_UPGRADE, ROTCC_UPGRADE_INFO,        _handle_upgrade_info         },
    { ROTCC_CMD_UPGRADE, ROTCC_UPGRADE_REQUEST,     _handle_upgrade_request      },
    { ROTCC_CMD_UPGRADE, ROTCC_UPGRADE_DATA,        _handle_upgrade_data         },
};


static rotcc_cmd_handler_t _find_ext_cmd(uint8_t cmd, uint8_t subcmd)
{
    for (uint32_t i = 0U; i < ROTCC_EXT_CMD_MAX; i++) {
        if ((ext_cmd_table[i].cmd == cmd) && (ext_cmd_table[i].subcmd == subcmd) && (ext_cmd_table[i].handler != NULL)) {
            return ext_cmd_table[i].handler;
        }
    }
    return NULL;
}

static rotcc_cmd_handler_t _find_builtin_cmd(uint8_t cmd, uint8_t subcmd)
{
    for (uint32_t i = 0U; i < (sizeof(cmd_table) / sizeof(cmd_table[0])); i++) {
        if ((cmd_table[i].cmd == cmd) && (cmd_table[i].subcmd == subcmd)) {
            return cmd_table[i].handler;
        }
    }
    return NULL;
}

static void _handle_msg(const rotcc_msg_t *msg)
{
    rotcc_cmd_handler_t handler = _find_builtin_cmd(msg->cmd, msg->subcmd);

    if (handler == NULL) {
        handler = _find_ext_cmd(msg->cmd, msg->subcmd);
    }
    if (handler == NULL) {
        _reply_status(msg, rotsta_not_supported);
        return;
    }
    handler(msg);
}

rotsta_t rotcc_proto_register_cmd(uint8_t cmd, uint8_t subcmd, rotcc_cmd_handler_t handler)
{
    rotcc_cmd_entry_t *free_entry = NULL;

    if (handler == NULL) {
        return rotsta_invalid_argument;
    }
    // Built-in commands always win, so overriding them is not allowed.
    if (_find_builtin_cmd(cmd, subcmd) != NULL) {
        return rotsta_not_supported;
    }

    for (uint32_t i = 0U; i < ROTCC_EXT_CMD_MAX; i++) {
        if (ext_cmd_table[i].handler == NULL) {
            if (free_entry == NULL) {
                free_entry = &ext_cmd_table[i];
            }
        }
        else if ((ext_cmd_table[i].cmd == cmd) && (ext_cmd_table[i].subcmd == subcmd)) {
            ext_cmd_table[i].handler = handler;
            return rotsta_success;
        }
    }

    if (free_entry == NULL) {
        return rotsta_fail;
    }
    free_entry->cmd = cmd;
    free_entry->subcmd = subcmd;
    free_entry->handler = handler;
    return rotsta_success;
}

rotsta_t rotcc_proto_unregister_cmd(uint8_t cmd, uint8_t subcmd)
{
    for (uint32_t i = 0U; i < ROTCC_EXT_CMD_MAX; i++) {
        if ((ext_cmd_table[i].handler != NULL) && (ext_cmd_table[i].cmd == cmd) && (ext_cmd_table[i].subcmd == subcmd)) {
            ext_cmd_table[i].handler = NULL;
            return rotsta_success;
        }
    }
    return rotsta_not_found;
}

static void _on_frame(const rotgen_frame_t *frame, void *ctx)
{
    rotcc_msg_t msg;

    (void)ctx;

    if (((frame->options & ROTGEN_OPT_REQUEST) == 0U) || (frame->payload_len < ROTCC_MSG_HEADER_SIZE)) {
        return;
    }

    msg.cmd = frame->payload[0];
    msg.subcmd = frame->payload[1];
    msg.reserve = rotutil_get_le16(&frame->payload[2]);
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
    rotcc_register_frame_cb(_on_frame);
    s_initialized = true;
    return rotsta_success;
}
