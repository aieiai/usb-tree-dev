#ifndef KEY_MAGNET_H
#define KEY_MAGNET_H

#include <stdint.h>

typedef union {
    uint32_t buf;
    struct {
        uint16_t top;
        uint16_t bot;
    };
    struct {
        uint16_t max;
        uint16_t min;
    };
} __attribute__((aligned(4))) kmag_lmt_t;

#endif  /* KEY_MAGNET_H */
