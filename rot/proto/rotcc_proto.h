// Rays Of Trace Control Center protocol
#ifndef ROTCC_PROTO_H
#define ROTCC_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rotsta.h"
#include "rotutil.h"


/* Rot-CC payload: cmd(1) subcmd(1) reserve(2) payload(n). */
#define ROTCC_PROTO_VERSION         (1U)
#define ROTCC_MSG_HEADER_SIZE       (4U)
#define ROTCC_UPGRADE_INFO_LEN      (48U)
#define ROTCC_UPGRADE_REQUEST_LEN   (14U)

/* Per-key commands: payload starts with {start, end}, followed by one entry per key. */
#define ROTCC_KC_RANGE_HEADER_LEN   (2U)
#define ROTCC_KC_CODE_LEN           (4U)
#define ROTCC_KC_TICK_LEN           (12U)
#define ROTCC_KC_TRG_LEN            (4U)
#define ROTCC_KC_DZ_LEN             (4U)
#define ROTCC_KC_NOM_LEN            (4U)
#define ROTCC_KC_RT_LEN             (6U)

enum {
    ROTCC_CMD_SYS = 0x00,
    ROTCC_CMD_UPGRADE = 0x01,
    ROTCC_CMD_KC = 0x02,
    ROTCC_CMD_AXIS = 0x03,
};

enum {
    ROTCC_SYS_HANDSHAKE = 0x01,
    ROTCC_SYS_INFO = 0x02,
    ROTCC_SYS_REBOOT = 0x03,
    ROTCC_SYS_RESET = 0x04,
    ROTCC_SYS_KEY_COUNT = 0x05,
};

enum {
    ROTCC_UPGRADE_INFO = 0x00,
    ROTCC_UPGRADE_REQUEST = 0x01,
    ROTCC_UPGRADE_DATA = 0x02,
};

enum {
    ROTCC_KC_GET = 0x00,
    ROTCC_KC_SET = 0x01,
    ROTCC_KC_LAYER_COUNT = 0x02,
    ROTCC_KC_ACTIVE_LAYER = 0x03,
    ROTCC_KC_SET_ACTIVE_LAYER = 0x04,
    ROTCC_KC_TICK_GET = 0x05,
    ROTCC_KC_TICK_SET = 0x06,
    ROTCC_KC_TRG_GET = 0x07,
    ROTCC_KC_TRG_SET = 0x08,
    ROTCC_KC_DZ_GET = 0x09,
    ROTCC_KC_DZ_SET = 0x0A,
    ROTCC_KC_NOM_GET = 0x0B,
    ROTCC_KC_NOM_SET = 0x0C,
    ROTCC_KC_RT_GET = 0x0D,
    ROTCC_KC_RT_SET = 0x0E,
};

enum {
    ROTCC_AXIS_LIST = 0x00,
    ROTCC_AXIS_SET = 0x01,
};

/* Request types carried by ROTCC_UPGRADE_REQUEST. */
enum {
    ROTCC_UPGRADE_REQ_START = 1,
    ROTCC_UPGRADE_REQ_STOP = 2,
    ROTCC_UPGRADE_REQ_FINISH = 3,
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

// Handler runs in the CC thread, so replies must not block.
typedef void (*rotcc_cmd_handler_t)(const rotcc_msg_t *msg);


// Binds the device protocol to rotcc. Call once after rotcc_init().
rotsta_t rotcc_proto_init(void);

// Builds a response with the request's cmd/subcmd/reserve.
rotsta_t rotcc_proto_reply(const rotcc_msg_t *msg, const void *payload, uint16_t payload_len);
rotsta_t rotcc_proto_register_cmd(uint8_t cmd, uint8_t subcmd, rotcc_cmd_handler_t handler);
rotsta_t rotcc_proto_unregister_cmd(uint8_t cmd, uint8_t subcmd);


#ifdef __cplusplus
}
#endif

#endif
