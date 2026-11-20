#ifndef RGB_ACTION_H
#define RGB_ACTION_H

#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t crc32;
    uint32_t version;
    uint8_t  name[32];

} __attribute__((aligned(4))) obr_t;

extern obr_t g_obr;

#endif
