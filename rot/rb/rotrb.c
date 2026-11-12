#include "rotrb.h"



bool rotrb_init(rotrb_t *rb, void *buf, rotrb_siz_t size)
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

void rotrb_reset(rotrb_t *rb)
{
    rb->in  = 0U;
    rb->out = 0U;
}

uint32_t rotrb_get_size(rotrb_t *rb)
{
    return rb->mask + 1U;
}

uint32_t rotrb_get_used(rotrb_t *rb)
{
    return rb->in - rb->out; // (rb->in - rb->out + rb->mask + 1U) & rb->mask;
}

uint32_t rotrb_get_free(rotrb_t *rb)
{
    return (rb->mask + 1U) - (rb->in - rb->out);
}

bool rotrb_is_empty(rotrb_t *rb)
{
    return (rb->in == rb->out);
}

bool rotrb_is_full(rotrb_t *rb)
{
    return (rb->in - rb->out) > rb->mask; // (rb->in == (rb->out + 1U) & rb->mask);
}

uint32_t rotrb_in(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    uint32_t offset = rb->in & rb->mask;
    uint32_t free   = (rb->mask + 1U) - (rb->in - rb->out);
    uint32_t size   = (size > free) ? free : size;

    remain = rb->mask + 1U - offset;
    remain = remain > size ? size : remain;

    memcpy(((uint8_t *)(rb->buf)) + offset, buf, remain);
    memcpy(rb->buf, (uint8_t *)buf + remain, size - remain);

    rb->in += size;
    return size;
}
uint32_t rotrb_cover(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    uint32_t offset = rb->in & rb->mask;
    uint32_t free   = (rb->mask + 1U) - (rb->in - rb->out);
    uint32_t size   = (size > free) ? (size > (rb->mask + 1)) ? (rb->mask + 1) : size : size;
    rb->out += (size > free) ? (size - free) : 0U;

    remain = rb->mask + 1U - offset;
    remain = remain > size ? size : remain;

    memcpy(((uint8_t *)(rb->buf)) + offset, buf, remain);
    memcpy(rb->buf, (uint8_t *)buf + remain, size - remain);

    rb->in += size;
    return size;
}

uint32_t rotrb_out(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    uint32_t offset = rb->out & rb->mask;
    uint32_t used   = rb->in - rb->out;
    uint32_t size   = (size > used) ? used : size;

    remain = rb->mask + 1U - offset;
    remain = remain > size ? size : remain;

    memcpy(buf, ((uint8_t *)(rb->buf)) + offset, remain);
    memcpy((uint8_t *)buf + remain, rb->buf, size - remain);

    rb->out += size;
    return size;
}

uint32_t rotrb_peek(rotrb_t *rb, void *buf, rotrb_siz_t size)
{
    size = rotrb_out(rb, buf, size);
    rb->out -= size;
    return size;
}

uint32_t rotrb_drop(rotrb_t *rb, rotrb_siz_t size)
{
    uint32_t used = rb->in - rb->out;
    size = (size > used) ? used : size;
    rb->out += size;
    return size;
}
