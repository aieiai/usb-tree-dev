#ifndef ROTRB_H
#define ROTRB_H

#ifdef __cplusplus
"C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define ROTRB_INIT(name, buf, size)  rotrb_t rotrb_##name = { .in = 0U, .out = 0U, .mask = size - 1U, .buf = buf }

typedef uint32_t rotrb_siz_t;

typedef struct {
    rotrb_siz_t in;
    rotrb_siz_t out;
    rotrb_siz_t mask;
    void *buf;
} rotrb_t;


static inline bool rotrb_init(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    if ((NULL == rb) || (NULL == buf)) {
        return false;
    }
    rb->in = 0U;
    rb->out = 0U;
    rb->mask = size - 1U;
    rb->buf = buf;
    return true;
}

static inline void rotrb_reset(rotrb_t *rb)
{
    rb->in  = 0U;
    rb->out = 0U;
}

static inline uint32_t rotrb_get_size(rotrb_t *rb)
{
    return rb->mask + 1U;
}

static inline uint32_t rotrb_get_used(rotrb_t *rb)
{
    return rb->in - rb->out; // (rb->in - rb->out + rb->mask + 1U) & rb->mask;
}

static inline uint32_t rotrb_get_free(rotrb_t *rb)
{
    return (rb->mask + 1U) - (rb->in - rb->out);
}

static inline bool rotrb_is_empty(rotrb_t *rb)
{
    return (rb->in == rb->out);
}

static inline bool rotrb_is_full(rotrb_t *rb)
{
    return (rb->in - rb->out) > rb->mask; // (rb->in == (rb->out + 1U) & rb->mask);
}

static inline uint32_t rotrb_in(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    uint32_t offset = rb->in & rb->mask;
    uint32_t free   = (rb->mask + 1U) - (rb->in - rb->out);
    uint32_t remain = rb->mask + 1U - offset;

    size   = (size > free) ? free : size;
    remain = remain > size ? size : remain;

    memcpy(((uint8_t *)(rb->buf)) + offset, buf, remain);
    memcpy(rb->buf, (uint8_t *)buf + remain, size - remain);

    rb->in += size;
    return size;
}

static inline uint32_t rotrb_cover(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    uint32_t offset = rb->in & rb->mask;
    uint32_t free   = (rb->mask + 1U) - (rb->in - rb->out);
    uint32_t remain = rb->mask + 1U - offset;

    rb->out += (size > free) ? (size - free) : 0U;
    size     = (size > free) ? (size > (rb->mask + 1)) ? (rb->mask + 1) : size : size;
    remain   = remain > size ? size : remain;

    memcpy(((uint8_t *)(rb->buf)) + offset, buf, remain);
    memcpy(rb->buf, (uint8_t *)buf + remain, size - remain);

    rb->in += size;
    return size;
}

static inline uint32_t rotrb_out(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    uint32_t offset = rb->out & rb->mask;
    uint32_t used   = rb->in - rb->out;
    uint32_t remain = rb->mask + 1U - offset;

    size   = (size > used) ? used : size;
    remain = remain > size ? size : remain;

    memcpy(buf, ((uint8_t *)(rb->buf)) + offset, remain);
    memcpy((uint8_t *)buf + remain, rb->buf, size - remain);

    rb->out += size;
    return size;
}

static inline uint32_t rotrb_peek(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    size = rotrb_out(rb, buf, size);
    rb->out -= size;
    return size;
}

static inline uint32_t rotrb_drop(rotrb_t *rb, rotrb_siz_t size)
{
    uint32_t used = rb->in - rb->out;
    size = (size > used) ? used : size;
    rb->out += size;
    return size;
}



#ifdef __cplusplus
}
#endif

#endif

