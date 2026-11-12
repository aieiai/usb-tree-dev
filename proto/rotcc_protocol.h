#ifndef ROTCC_PROTOCOL_H
#define ROTCC_PROTOCOL_H

#include <stdint.h>

#include "rotsta.h"
#ifndef INTF_CC_EP_SIZE
#include "../hal/usb/descriptor/hid_descriptor.h"
#endif


/*
 * Wire frame:
 *   0..1 magic "RC", 2 version, 3 type, 4..5 sequence,
 *   6..7 payload length, 8..N payload, N..N+1 CRC16-CCITT.
 */
#define ROTCC_FW_MAGIC_0            (0x52U)
#define ROTCC_FW_MAGIC_1            (0x43U)
#define ROTCC_FW_VERSION            (1U)
#define ROTCC_FW_HEADER_SIZE        (8U)
#define ROTCC_FW_CRC_SIZE           (2U)
#define ROTCC_FW_MAX_PACKET_SIZE    (INTF_CC_EP_SIZE)
#define ROTCC_FW_MAX_PAYLOAD_SIZE   (ROTCC_FW_MAX_PACKET_SIZE - \
                                     ROTCC_FW_HEADER_SIZE - ROTCC_FW_CRC_SIZE)


enum {
    ROTCC_FW_TYPE_START = 0x10,
    ROTCC_FW_TYPE_DATA = 0x11,
    ROTCC_FW_TYPE_END = 0x12,
    ROTCC_FW_TYPE_ABORT = 0x13,
    ROTCC_FW_TYPE_QUERY = 0x20,
    ROTCC_FW_TYPE_ACK = 0x80,
    ROTCC_FW_TYPE_NAK = 0x81,
    ROTCC_FW_TYPE_INFO = 0x82,
};


typedef struct {
    uint8_t type;
    uint16_t seq;
    uint16_t payload_len;
    const uint8_t *payload;
} rotcc_protocol_frame_t;


/*
 * Parses and validates one protocol frame. The payload points into the input
 * buffer and remains valid only during the call.
 * rotsta_not_found means "not a CC protocol frame, ignore it".
 */
rotsta_t rotcc_protocol_parse(const uint8_t *data, uint16_t len,
                                 rotcc_protocol_frame_t *frame);

rotsta_t rotcc_protocol_send_status(uint8_t type, uint16_t seq,
                                       rotsta_t status);
rotsta_t rotcc_protocol_send_payload(uint8_t type, uint16_t seq,
                                        const uint8_t *payload,
                                        uint16_t payload_len);

uint32_t rotcc_protocol_get_le32(const uint8_t *data);


#endif
