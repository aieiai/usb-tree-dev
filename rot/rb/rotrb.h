#ifndef ROTRB_H
#define ROTRB_H

#ifdef __cplusplus
"C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define ROTRB_INIT(name, buf, size)  rotrb_t rotrb_##name = { .in = 0U, .out = 0U, .mask = size - 1U, .buf = buf }



typedef uint32_t rotrb_siz_t;

typedef struct {
    rotrb_siz_t in;
    rotrb_siz_t out;
    rotrb_siz_t mask;
    void *buf;
} rotrb_t;



bool rotrb_init(rotrb_t *rb, void *buf, rotrb_siz_t size);
void rotrb_reset(rotrb_t *rb);
// void rotrb_reset_read(rotrb_t *rb);

uint32_t rotrb_get_used(rotrb_t *rb);
uint32_t rotrb_get_size(rotrb_t *rb);
uint32_t rotrb_get_free(rotrb_t *rb);

bool rotrb_is_full(rotrb_t *rb);
bool rotrb_is_empty(rotrb_t *rb);

// bool rotrb_write_byte(rotrb_t *rb, uint8_t byte);
// bool rotrb_overwrite_byte(rotrb_t *rb, uint8_t byte);
// bool rotrb_peek_byte(rotrb_t *rb, uint8_t *byte);
// bool rotrb_read_byte(rotrb_t *rb, uint8_t *byte);
// bool rotrb_drop_byte(rotrb_t *rb);

uint32_t rotrb_in(rotrb_t *rb, void *buf, rotrb_siz_t size);
uint32_t rotrb_out(rotrb_t *rb, void *buf, rotrb_siz_t size);
uint32_t rotrb_peek(rotrb_t *rb, void *buf, rotrb_siz_t size);
uint32_t rotrb_drop(rotrb_t *rb, rotrb_siz_t size);
uint32_t rotrb_cover(rotrb_t *rb, void *buf, rotrb_siz_t size);

// void *rotrb_linear_write_setup(rotrb_t *rb, rotrb_siz_t *size);
// void *rotrb_linear_read_setup(rotrb_t *rb, rotrb_siz_t *size);
// uint32_t rotrb_linear_write_done(rotrb_t *rb, rotrb_siz_t size);
// uint32_t rotrb_linear_read_done(rotrb_t *rb, rotrb_siz_t size);


#ifdef __cplusplus
}
#endif

#endif

