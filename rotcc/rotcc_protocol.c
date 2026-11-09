#include "rotcc_protocol.h"

#include <stddef.h>

#include "rotcc.h"


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
            crc = (crc & 0x8000U)
                ? (uint16_t)((crc << 1) ^ 0x1021U)
                : (uint16_t)(crc << 1);
        }
    }
    return crc;
}


rot_state_t rotcc_protocol_parse(const uint8_t *data, uint16_t len,
                                 rotcc_protocol_frame_t *frame)
{
    uint16_t payload_len;
    uint16_t expected_crc;
    uint16_t actual_crc;
    uint32_t packet_len;

    if ((data == NULL) || (frame == NULL)) {
        return rot_state_invalid_argument;
    }
    if ((len < 2U) ||
        (data[0] != ROTCC_FW_MAGIC_0) ||
        (data[1] != ROTCC_FW_MAGIC_1)) {
        return rot_state_not_found;
    }
    if (len < (ROTCC_FW_HEADER_SIZE + ROTCC_FW_CRC_SIZE)) {
        return rot_state_invalid_argument;
    }

    frame->type = data[3];
    frame->seq = _get_le16(&data[4]);
    payload_len = _get_le16(&data[6]);
    if ((data[2] != ROTCC_FW_VERSION) ||
        (payload_len > ROTCC_FW_MAX_PAYLOAD_SIZE)) {
        return rot_state_not_supported;
    }

    packet_len = ROTCC_FW_HEADER_SIZE + payload_len + ROTCC_FW_CRC_SIZE;
    if (packet_len != len) {
        return rot_state_invalid_argument;
    }

    expected_crc = _get_le16(&data[packet_len - ROTCC_FW_CRC_SIZE]);
    actual_crc = _crc16_update(0U, data,
                               ROTCC_FW_HEADER_SIZE + payload_len);
    if (expected_crc != actual_crc) {
        return rot_state_invalid_argument;
    }

    frame->payload_len = payload_len;
    frame->payload = &data[ROTCC_FW_HEADER_SIZE];
    return rot_state_success;
}


rot_state_t rotcc_protocol_send_payload(uint8_t type, uint16_t seq,
                                        const uint8_t *payload,
                                        uint16_t payload_len)
{
    uint8_t packet[ROTCC_FW_MAX_PACKET_SIZE];
    uint16_t packet_len;
    uint16_t crc;

    if ((payload_len > ROTCC_FW_MAX_PAYLOAD_SIZE) ||
        ((payload == NULL) && (payload_len != 0U))) {
        return rot_state_invalid_argument;
    }

    packet_len = ROTCC_FW_HEADER_SIZE + payload_len + ROTCC_FW_CRC_SIZE;
    packet[0] = ROTCC_FW_MAGIC_0;
    packet[1] = ROTCC_FW_MAGIC_1;
    packet[2] = ROTCC_FW_VERSION;
    packet[3] = type;
    _put_le16(&packet[4], seq);
    _put_le16(&packet[6], payload_len);
    if (payload_len != 0U) {
        for (uint16_t i = 0U; i < payload_len; i++) {
            packet[ROTCC_FW_HEADER_SIZE + i] = payload[i];
        }
    }

    crc = _crc16_update(0U, packet,
                        ROTCC_FW_HEADER_SIZE + payload_len);
    _put_le16(&packet[packet_len - ROTCC_FW_CRC_SIZE], crc);
    return rotcc_send(packet, packet_len);
}


rot_state_t rotcc_protocol_send_status(uint8_t type, uint16_t seq,
                                       rot_state_t status)
{
    uint8_t payload[sizeof(uint32_t)];

    _put_le32(payload, status);
    return rotcc_protocol_send_payload(type, seq, payload, sizeof(payload));
}


uint32_t rotcc_protocol_get_le32(const uint8_t *data)
{
    return _get_le32(data);
}
