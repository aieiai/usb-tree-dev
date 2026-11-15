// Rays Of Trace Control Center protocol
#ifndef ROTCC_PROTO_H
#define ROTCC_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rotsta.h"


/* Rot-CC payload: cmd(1) subcmd(1) reserve(2) payload(n). */
#define ROTCC_PROTO_VERSION     (1U)
#define ROTCC_MSG_HEADER_SIZE   (4U)

enum {
    ROTCC_CMD_SYS = 0x00,
    ROTCC_CMD_KC = 0x01,
    ROTCC_CMD_UPGRADE = 0x02,
};

enum {
    ROTCC_SYS_VERSION = 0x00,
    ROTCC_SYS_HANDSHAKE,
    ROTCC_SYS_INFO,
    ROTCC_SYS_REBOOT,
    ROTCC_SYS_RESET,
};

enum {
    ROTCC_KC_GET = 0x00,
    ROTCC_KC_SET,
};

enum {
    ROTCC_UPGRADE_QUERY = 0x00,
    ROTCC_UPGRADE_START,
    ROTCC_UPGRADE_DATA,
    ROTCC_UPGRADE_END,
    ROTCC_UPGRADE_ABORT,
};

/* reset options, see the Rot-CC specification */
enum {
    ROTCC_RESET_KC      = (1UL << 0),
    ROTCC_RESET_RGB     = (1UL << 1),
    ROTCC_RESET_ADV_MT  = (1UL << 2),
    ROTCC_RESET_ALL     = 0xFFFFFFFFUL,
};


typedef struct {
    uint8_t cmd;
    uint8_t subcmd;
    uint16_t reserve;
    uint16_t payload_len;
    const uint8_t *payload;
} rotcc_msg_t;


/* Binds the device protocol to rotcc. Call once after rotcc_init(). */
rotsta_t rotcc_proto_init(void);


#ifdef __cplusplus
}
#endif

#endif
