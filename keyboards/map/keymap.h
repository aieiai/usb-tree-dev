#ifndef KEY_MAP_H
#define KEY_MAP_H

#include "kbdconfig.h"
#include "rotsta.h"

// 
typedef struct {
    uint32_t magic;
    uint8_t  name[16];
    uint8_t  key_max;
    uint8_t  sys_max;
    uint8_t  map_max;
    uint8_t  map_cur;
    uint32_t crc32;

    uint32_t keymap[KBD_SYS_MAX][KBD_MAP_MAX][KBD_KEY_MAX];
} __attribute__ ((aligned (4))) obmap_t;

extern obmap_t g_obmap;

rotsta_t keymap_init(void);
rotsta_t keymap_reset(void);
const uint32_t *keymap_get_default(uint8_t sys, uint8_t map);
uint32_t keymap_get_key(uint8_t layer, uint8_t key);
rotsta_t keymap_set_key(uint8_t layer, uint8_t key, uint32_t code);
uint8_t keymap_get_active_layer(void);
rotsta_t keymap_set_active_layer(uint8_t layer);

#endif
