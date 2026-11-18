// Rays Of Trace Generic Protocol
#ifndef ROTGEN_PROTO_H
#define ROTGEN_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rotsta.h"
#include "rotutil.h"


/*
 * ,----------------------------------------------------------------------------------------------------------------------------------------------------------.
 * | Rot-Gen frame:                                                                                                                                           |
 * |----------+------------+------------+-------------+-------------+-------------------+---------------+----------------+------------------+-----------------|
 * | sof 'R'  |   version  |   options  |   src_addr  |   dst_addr  |   payload_length  |   secret_key  |   header_crc8  |   package_crc16  |   payload ...   |
 * |----------|------------|------------|-------------|-------------|-------------------|---------------|----------------|------------------|-----------------|
 * |    0     |     1      |     2      |      3      |      4      |        5..6       |     7..8      |        9       |      10..11      |      12..N      |
 * `----------------------------------------------------------------------------------------------------------------------------------------------------------'
 */

#define ROTGEN_SOF_IDX          (0U)
#define ROTGEN_SOF_SIZE         (1U)
#define ROTGEN_VER_IDX          (ROTGEN_SOF_IDX + ROTGEN_SOF_SIZE)
#define ROTGEN_VER_SIZE         (1U)
#define ROTGEN_OPT_IDX          (ROTGEN_VER_IDX + ROTGEN_VER_SIZE)
#define ROTGEN_OPT_SIZE         (1U)
#define ROTGEN_SRC_IDX          (ROTGEN_OPT_IDX + ROTGEN_OPT_SIZE)
#define ROTGEN_SRC_SIZE         (1U)
#define ROTGEN_DST_IDX          (ROTGEN_SRC_IDX + ROTGEN_SRC_SIZE)
#define ROTGEN_DST_SIZE         (1U)
#define ROTGEN_PLD_LEN_IDX      (ROTGEN_DST_IDX + ROTGEN_DST_SIZE)
#define ROTGEN_PLD_LEN_SIZ      (2U)
#define ROTGEN_KEK_IDX          (ROTGEN_PLD_LEN_IDX + ROTGEN_PLD_LEN_SIZ)
#define ROTGEN_KEK_SIZE         (2U)
#define ROTGEN_HDR_CRC8_IDX     (ROTGEN_KEK_IDX + ROTGEN_KEK_SIZE)
#define ROTGEN_HDR_CRC8_SIZ     (1U)
#define ROTGEN_PKG_CRC16_IDX    (ROTGEN_HDR_CRC8_IDX + ROTGEN_HDR_CRC8_SIZ)
#define ROTGEN_PKG_CRC16_SIZ    (2U)
#define ROTGEN_PLD_IDX          (ROTGEN_PKG_CRC16_IDX + ROTGEN_PKG_CRC16_SIZ)

#define ROTGEN_SOF              ('R')
#define ROTGEN_VER              (1U)
#define ROTGEN_KEK              (0U)
#define ROTGEN_HEADER_SIZE      (12U)

_Static_assert((ROTGEN_PLD_IDX) == ROTGEN_HEADER_SIZE, "");

/* Upper bound for one frame, the transport decides the real buffer size. */
#ifndef ROTGEN_MAX_FRAME_SIZE
#define ROTGEN_MAX_FRAME_SIZE       (1024U)
#endif
#define ROTGEN_MAX_PAYLOAD_SIZE     (ROTGEN_MAX_FRAME_SIZE - ROTGEN_HEADER_SIZE)


enum {
    ROTGEN_OPT_ENCRYPT   = (1U << 0),
    ROTGEN_OPT_FRAGMENT  = (1U << 1),
    ROTGEN_OPT_BROADCAST = (1U << 2),
    ROTGEN_OPT_REQUEST   = (1U << 3),
};

enum {
    ROTGEN_ADDR_CC = 0,     /* Rot control center */
    ROTGEN_ADDR_KEYBOARD,
    ROTGEN_ADDR_MOUSE,
    ROTGEN_ADDR_HUB,
    ROTGEN_ADDR_HEADSET,
    ROTGEN_ADDR_MAX,
};


typedef struct {
    uint8_t version;
    uint8_t options;
    uint8_t src;
    uint8_t dst;
    uint16_t payload_len;
    const uint8_t *payload; /* valid only during the callback */
} rotgen_frame_t;


/* Transport primitives, both sides are supplied by the caller. */
typedef rotsta_t (*rotgen_send_fn_t)(const uint8_t *data, uint16_t len, void *ctx);
typedef void (*rotgen_frame_fn_t)(const rotgen_frame_t *frame, void *ctx);


typedef struct {
    rotgen_send_fn_t  send;
    rotgen_frame_fn_t on_frame;
    void    *ctx;
    uint8_t  addr;
    uint16_t rx_len;
    uint16_t rx_cap;
    uint8_t *rx_buf;

    volatile uint32_t dropped; /* bytes discarded while resynchronising */
    volatile uint32_t errors;  /* frames that failed the package CRC */
} rotgen_t;


/*
 * Binds one protocol instance to its transport. rx_buf is the reassembly
 * buffer and also limits the accepted payload length.
 */
bool rotgen_init(rotgen_t *g, void *rx_buf, uint16_t rx_buf_size, uint8_t addr, rotgen_send_fn_t send, rotgen_frame_fn_t on_frame, void *ctx);

/*
 * Consumes arbitrary transport chunks. Complete, CRC valid frames are handed to
 * on_frame(); corrupt or partial data is dropped byte by byte until the stream
 * is synchronised again.
 */
void rotgen_feed(rotgen_t *g, const void *data, uint16_t len);

/* Encodes one frame and hands it to the registered send callback. */
rotsta_t rotgen_send(rotgen_t *g, uint8_t options, uint8_t dst, const void *payload, uint16_t payload_len);



#ifdef __cplusplus
}
#endif

#endif
