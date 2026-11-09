#ifndef ROTCC_UPGRADE_H
#define ROTCC_UPGRADE_H

#include <stdint.h>


/*
 * Firmware transfer protocol carried by the CC interface only.
 *
 * Every CC packet is at most INTF_CC_EP_SIZE (64) bytes:
 *   0..1   magic "RC" (0x52, 0x43)
 *   2      protocol version (1)
 *   3      packet type
 *   4..5   sequence, little endian
 *   6..7   payload length, little endian, max 54
 *   8..    payload
 *   end-2  CRC16-CCITT, little endian, over header and payload
 *
 * START payload: file_size (u32 LE), session_id (u32 LE)
 * ACK/NAK payload: rot_state_t status (u32 LE)
 * DATA sequence starts at zero and wraps after 0xffff.
 *
 * DT is intentionally not used by this protocol.
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
    ROTCC_FW_TYPE_ACK = 0x80,
    ROTCC_FW_TYPE_NAK = 0x81,
};


void rotcc_upgrade_on_rx(const uint8_t *data, uint16_t len);


#endif
