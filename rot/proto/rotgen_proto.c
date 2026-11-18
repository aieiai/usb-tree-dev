#include "rotgen_proto.h"

#include <stddef.h>
#include <string.h>
#include "rotcrc.h"



_Static_assert(ROTGEN_MAX_FRAME_SIZE > ROTGEN_HEADER_SIZE, "frame buffer must hold the Rot-Gen header");
_Static_assert(ROTGEN_MAX_FRAME_SIZE <= 0xFFFFU, "frame size must fit the 16-bit payload length field");



static uint16_t _frame_len(const uint8_t *header)
{
    return (uint16_t)(ROTGEN_HEADER_SIZE + rotutil_get_le16(&header[ROTGEN_PLD_LEN_IDX]));
}

static bool _header_valid(const rotgen_t *g)
{
    const uint8_t *header = g->rx_buf;

    if ((header[ROTGEN_SOF_IDX] != ROTGEN_SOF) || (header[ROTGEN_VER_IDX] != ROTGEN_VER)) {
        return false;
    }
    if (rotcrc8_update(0, header, ROTGEN_HDR_CRC8_IDX) != header[ROTGEN_HDR_CRC8_IDX]) {
        return false;
    }
    return _frame_len(header) <= g->rx_cap;
}

/* Drops one byte so the next feed can look for a new start of frame. */
static void _resync(rotgen_t *g)
{
    g->rx_len--;
    memmove(g->rx_buf, &g->rx_buf[1], g->rx_len);
    g->dropped++;
}

static void _deliver(rotgen_t *g)
{
    rotgen_frame_t frame;
    uint16_t payload_len = rotutil_get_le16(&g->rx_buf[ROTGEN_PLD_LEN_IDX]);
    uint16_t crc = rotutil_get_le16(&g->rx_buf[ROTGEN_PKG_CRC16_IDX]);
    uint16_t calc;

    calc = rotcrc16_update(0U, g->rx_buf, (ROTGEN_HEADER_SIZE - ROTGEN_PKG_CRC16_SIZ));
    calc = rotcrc16_update(calc, &g->rx_buf[ROTGEN_PLD_IDX], payload_len);
    if (crc != calc) {
        g->errors++;
        return;
    }

    frame.version = g->rx_buf[ROTGEN_VER_IDX];
    frame.options = g->rx_buf[ROTGEN_OPT_IDX];
    frame.src = g->rx_buf[ROTGEN_SRC_IDX];
    frame.dst = g->rx_buf[ROTGEN_DST_IDX];
    frame.payload_len = payload_len;
    frame.payload = &g->rx_buf[ROTGEN_PLD_IDX];

    if ((frame.dst != g->addr) && ((frame.options & ROTGEN_OPT_BROADCAST) == 0U)) {
        return;
    }
    if (g->on_frame != NULL) {
        g->on_frame(&frame, g->ctx);
    }
}

bool rotgen_init(rotgen_t *g, void *rx_buf, uint16_t rx_buf_size, uint8_t addr, rotgen_send_fn_t send, rotgen_frame_fn_t on_frame, void *ctx)
{
    if ((g == NULL) || (rx_buf == NULL) || (rx_buf_size <= ROTGEN_HEADER_SIZE)) {
        return false;
    }

    memset(g, 0, sizeof(*g));
    g->rx_buf = (uint8_t *)rx_buf;
    g->rx_cap = rx_buf_size;
    g->addr = addr;
    g->send = send;
    g->on_frame = on_frame;
    g->ctx = ctx;
    return true;
}

void rotgen_feed(rotgen_t *g, const void *data, uint16_t len)
{
    const uint8_t *src = (const uint8_t *)data;

    if ((g == NULL) || (src == NULL) || (len == 0U) || (g->rx_buf == NULL)) {
        return;
    }

    while (len > 0U) {
        uint16_t total;
        uint16_t copy;

        if (g->rx_len < ROTGEN_HEADER_SIZE) {
            copy = (uint16_t)(ROTGEN_HEADER_SIZE - g->rx_len);
            if (copy > len) {
                copy = len;
            }
            memcpy(&g->rx_buf[g->rx_len], src, copy);
            g->rx_len = (uint16_t)(g->rx_len + copy);
            src += copy;
            len = (uint16_t)(len - copy);
            if (g->rx_len < ROTGEN_HEADER_SIZE) {
                return;
            }
            if (!_header_valid(g)) {
                _resync(g);
                continue;
            }
        }

        total = _frame_len(g->rx_buf);
        copy = (uint16_t)(total - g->rx_len);
        if (copy > len) {
            copy = len;
        }
        memcpy(&g->rx_buf[g->rx_len], src, copy);
        g->rx_len = (uint16_t)(g->rx_len + copy);
        src += copy;
        len = (uint16_t)(len - copy);
        if (g->rx_len < total) {
            return;
        }

        _deliver(g);
        g->rx_len = 0U;
    }
}

rotsta_t rotgen_send(rotgen_t *g, uint8_t options, uint8_t dst, const void *payload, uint16_t payload_len)
{
    uint8_t frame[ROTGEN_MAX_FRAME_SIZE];
    uint16_t total = (uint16_t)(ROTGEN_HEADER_SIZE + payload_len);
    uint16_t crc;

    if ((g == NULL) || (g->send == NULL)) {
        return rotsta_invalid_argument;
    }
    if ((payload == NULL) && (payload_len != 0U)) {
        return rotsta_invalid_argument;
    }
    if ((total > ROTGEN_MAX_FRAME_SIZE) || (total > g->rx_cap)) {
        return rotsta_invalid_argument;
    }

    frame[ROTGEN_SOF_IDX] = ROTGEN_SOF;
    frame[ROTGEN_VER_IDX] = ROTGEN_VER;
    frame[ROTGEN_OPT_IDX] = options;
    frame[ROTGEN_SRC_IDX] = g->addr;
    frame[ROTGEN_DST_IDX] = dst;
    rotutil_put_le16(&frame[ROTGEN_KEK_IDX], ROTGEN_KEK);
    rotutil_put_le16(&frame[ROTGEN_PLD_LEN_IDX], payload_len);
    frame[ROTGEN_HDR_CRC8_IDX] = rotcrc8_update(0, frame, ROTGEN_HDR_CRC8_IDX);
    if (payload_len != 0U) {
        memcpy(&frame[ROTGEN_PLD_IDX], payload, payload_len);
    }

    crc = rotcrc16_update(0U, frame, (ROTGEN_HEADER_SIZE - ROTGEN_PKG_CRC16_SIZ));
    crc = rotcrc16_update(crc, &frame[ROTGEN_PLD_IDX], payload_len);
    rotutil_put_le16(&frame[ROTGEN_PKG_CRC16_IDX], crc);

    return g->send(frame, total, g->ctx);
}
