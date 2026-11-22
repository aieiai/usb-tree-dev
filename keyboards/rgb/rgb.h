/**
 * @Author: wj
 * @Brief:  rgb .
 * @Date:   2024/07/02
 */

#ifndef RGB_H
#define RGB_H

#include <stdio.h>
#include <stdint.h>
#include "rotsta.h"

typedef struct {
    uint32_t magic;
    uint32_t crc32;
    uint32_t version;
    uint8_t  name[32];

} __attribute__((aligned(4))) obr_t;

extern obr_t g_obr;

rotsta_t rgb_init(void);

#endif  /* RGB_H */

