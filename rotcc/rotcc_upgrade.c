#include "rotcc_upgrade.h"

#include <stdbool.h>

#include "rotcc.h"
#include "../hal/usb/descriptor/hid_descriptor.h"
#include "../hal/usb/diagnostic_tracking/hal_dt.h"
#include "../upgrade/upgrade.h"


#define TAG "ccfw: "
#define ROTCC_FW_START_PAYLOAD_SIZE (8U)
#define ROTCC_FW_STATUS_PAYLOAD_SIZE (4U)
#define ROTCC_FW_MAX_FILE_SIZE \
    (UPGRADE_IMAGE_MAX_SIZE + sizeof(upgrade_image_header_t))

_Static_assert(ROTCC_FW_MAX_PACKET_SIZE == INTF_CC_EP_SIZE,
               "CC firmware protocol packet must match the CC endpoint size");


static bool s_active;
static bool s_verified;
static uint32_t s_session_id;
static uint32_t s_file_size;
static uint32_t s_received_size;
static uint16_t s_next_data_seq;


static uint16_t _get_le16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}


static uint32_t _get_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
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


static uint16_t _crc16_update(uint16_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) :
                                    (uint16_t)(crc << 1);
        }
    }
    return crc;
}


static void _send_status(uint8_t type, uint16_t seq, rot_state_t status)
{
    uint8_t packet[ROTCC_FW_MAX_PACKET_SIZE];
    uint32_t packet_len = ROTCC_FW_HEADER_SIZE +
                          ROTCC_FW_STATUS_PAYLOAD_SIZE +
                          ROTCC_FW_CRC_SIZE;
    uint16_t crc;

    packet[0] = ROTCC_FW_MAGIC_0;
    packet[1] = ROTCC_FW_MAGIC_1;
    packet[2] = ROTCC_FW_VERSION;
    packet[3] = type;
    _put_le16(&packet[4], seq);
    _put_le16(&packet[6], ROTCC_FW_STATUS_PAYLOAD_SIZE);
    _put_le32(&packet[8], status);

    crc = _crc16_update(0U, packet,
                        ROTCC_FW_HEADER_SIZE + ROTCC_FW_STATUS_PAYLOAD_SIZE);
    _put_le16(&packet[packet_len - ROTCC_FW_CRC_SIZE], crc);
    (void)rotcc_send(packet, packet_len);
}


static void _send_ack(uint16_t seq)
{
    _send_status(ROTCC_FW_TYPE_ACK, seq, rot_state_success);
}


static void _send_nak(uint16_t seq, rot_state_t state)
{
    _send_status(ROTCC_FW_TYPE_NAK, seq, state);
}


static void _reset_session(void)
{
    if (s_active) {
        (void)upgrade_abort();
    }
    s_active = false;
    s_verified = false;
    s_session_id = 0U;
    s_file_size = 0U;
    s_received_size = 0U;
    s_next_data_seq = 0U;
}


static void _handle_start(uint16_t seq, const uint8_t *payload, uint16_t payload_len)
{
    uint32_t file_size;
    uint32_t session_id;
    rot_state_t state;

    if (payload_len != ROTCC_FW_START_PAYLOAD_SIZE) {
        _send_nak(seq, rot_state_invalid_argument);
        return;
    }

    file_size = _get_le32(payload);
    session_id = _get_le32(&payload[4]);
    if (file_size < sizeof(upgrade_image_header_t)) {
        _send_nak(seq, rot_state_invalid_argument);
        return;
    }
    if (file_size > ROTCC_FW_MAX_FILE_SIZE) {
        _send_nak(seq, rot_state_upgrade_image_too_large);
        return;
    }

    /* A repeated START means the previous ACK was lost. Do not erase twice. */
    if (s_active && !s_verified &&
        (s_session_id == session_id) &&
        (s_file_size == file_size) &&
        (s_received_size == 0U) &&
        (s_next_data_seq == 0U)) {
        _send_ack(seq);
        return;
    }

    _reset_session();
    state = upgrade_init();
    if (state != rot_state_success) {
        hal_dt_printf(TAG "upgrade init failed: 0x%08x\r\n", (unsigned)state);
        _send_nak(seq, state);
        return;
    }

    s_session_id = session_id;
    s_file_size = file_size;
    s_received_size = 0U;
    s_next_data_seq = 0U;
    s_verified = false;
    s_active = true;

    hal_dt_printf(TAG "start session=0x%08x, size=%u\r\n",
                  (unsigned)session_id, (unsigned)file_size);
    _send_ack(seq);
}


static void _handle_data(uint16_t seq, const uint8_t *payload, uint16_t payload_len)
{
    bool image_complete = false;
    rot_state_t state;

    if (!s_active || s_verified) {
        _send_nak(seq, rot_state_upgrade_incomplete);
        return;
    }

    /* ACK a retransmission whose previous response was lost. */
    if ((s_next_data_seq != 0U) &&
        (seq == (uint16_t)(s_next_data_seq - 1U))) {
        _send_ack(seq);
        return;
    }
    if (seq != s_next_data_seq) {
        _send_nak(seq, rot_state_upgrade_incomplete);
        return;
    }
    if ((payload_len == 0U) ||
        (payload_len > ROTCC_FW_MAX_PAYLOAD_SIZE) ||
        (payload_len > (s_file_size - s_received_size))) {
        _send_nak(seq, rot_state_invalid_argument);
        return;
    }

    state = upgrade_feed(payload, payload_len, &image_complete);
    if (state != rot_state_success) {
        hal_dt_printf(TAG "data seq=%u failed: 0x%08x\r\n",
                      (unsigned)seq, (unsigned)state);
        _send_nak(seq, state);
        return;
    }

    s_received_size += payload_len;
    s_next_data_seq++;
    if (((s_next_data_seq & 0x3fU) == 0U) || image_complete) {
        hal_dt_printf(TAG "rx %u/%u bytes%s\r\n",
                      (unsigned)s_received_size, (unsigned)s_file_size,
                      image_complete ? " (image complete)" : "");
    }
    _send_ack(seq);
}


static void _handle_end(uint16_t seq)
{
    rot_state_t state;

    if (!s_active) {
        _send_nak(seq, rot_state_upgrade_incomplete);
        return;
    }
    if (s_verified) {
        _send_ack(seq);
        return;
    }
    if (s_received_size != s_file_size) {
        _send_nak(seq, rot_state_upgrade_incomplete);
        return;
    }

    state = upgrade_finish();
    if (state != rot_state_success) {
        hal_dt_printf(TAG "finish failed: 0x%08x\r\n", (unsigned)state);
        _send_nak(seq, state);
        return;
    }

    s_verified = true;
    hal_dt_printf(TAG "verified %u bytes\r\n", (unsigned)s_received_size);
    _send_ack(seq);
}


static void _handle_abort(uint16_t seq)
{
    _reset_session();
    hal_dt_printf(TAG "aborted by host\r\n");
    _send_ack(seq);
}


void rotcc_upgrade_on_rx(const uint8_t *data, uint16_t len)
{
    uint16_t payload_len;
    uint16_t seq;
    uint16_t expected_crc;
    uint16_t actual_crc;
    uint32_t packet_len;
    uint8_t type;

    if ((data == NULL) ||
        (len < (ROTCC_FW_HEADER_SIZE + ROTCC_FW_CRC_SIZE)) ||
        (data[0] != ROTCC_FW_MAGIC_0) ||
        (data[1] != ROTCC_FW_MAGIC_1)) {
        return;
    }

    seq = _get_le16(&data[4]);
    payload_len = _get_le16(&data[6]);
    type = data[3];
    if ((data[2] != ROTCC_FW_VERSION) ||
        (payload_len > ROTCC_FW_MAX_PAYLOAD_SIZE)) {
        _send_nak(seq, rot_state_not_supported);
        return;
    }

    packet_len = ROTCC_FW_HEADER_SIZE + payload_len + ROTCC_FW_CRC_SIZE;
    if (packet_len != len) {
        _send_nak(seq, rot_state_invalid_argument);
        return;
    }

    expected_crc = _get_le16(&data[packet_len - ROTCC_FW_CRC_SIZE]);
    actual_crc = _crc16_update(0U, data,
                               ROTCC_FW_HEADER_SIZE + payload_len);
    if (expected_crc != actual_crc) {
        _send_nak(seq, rot_state_invalid_argument);
        return;
    }

    switch (type) {
    case ROTCC_FW_TYPE_START:
        _handle_start(seq, &data[ROTCC_FW_HEADER_SIZE], payload_len);
        break;
    case ROTCC_FW_TYPE_DATA:
        _handle_data(seq, &data[ROTCC_FW_HEADER_SIZE], payload_len);
        break;
    case ROTCC_FW_TYPE_END:
        _handle_end(seq);
        break;
    case ROTCC_FW_TYPE_ABORT:
        _handle_abort(seq);
        break;
    default:
        _send_nak(seq, rot_state_not_supported);
        break;
    }
}


void rot_cc_on_rx_packet(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    rotcc_upgrade_on_rx(data, len);
}
