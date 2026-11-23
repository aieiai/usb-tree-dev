#ifndef KEY_MAP_H
#define KEY_MAP_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "kbd_config.h"
#include "rotsta.h"

#define MAGIC_OBKM 0x424D4B5F /* OBKM */
#define OBKM_VER   0U

// on-board mapping
typedef struct {
    uint32_t magic;
    uint32_t crc32;
    uint32_t version;
    uint8_t  name[32];
    
    uint8_t  sys_max;
    uint8_t  sys_cur;
    uint8_t  map_max;
    uint8_t  map_cur;
    uint8_t  key_max;
    uint8_t  layout_max; // 预留配列布局
    uint8_t  layout_cur;

    uint32_t keymap[KBD_SYS_MAX][KBD_MAP_MAX][KBD_KEY_MAX];
} __attribute__ ((aligned (4))) obm_t;

extern obm_t g_obm;

rotsta_t obm_db_register(void);
rotsta_t keymap_init(void);
rotsta_t keymap_reset(void);
const uint32_t *keymap_get_default(uint8_t sys, uint8_t map);
uint32_t keymap_get_key(uint8_t layer, uint8_t key);
rotsta_t keymap_set_key(uint8_t layer, uint8_t key, uint32_t code);
uint8_t keymap_get_active_layer(void);
rotsta_t keymap_set_active_layer(uint8_t layer);
uint8_t keymap_get_active_sys(void);
rotsta_t keymap_set_active_sys(uint8_t sys);

#endif
