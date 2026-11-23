#ifndef KEY_MAGNET_H
#define KEY_MAGNET_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "rotsta.h"
#include "kbd_config.h"

// Distance unit: um
#ifndef DD
#   ifdef KBD_DD
#       define DD   (KBD_DD)
#   else
#       define DD   (1)
#   endif
#endif

#ifndef UM2MM
#   ifdef KBD_UM2MM
#       define UM2MM(um)    (KBD_UM2MM(um))
#   else
#       define UM2MM(um)    ((float)(um) * 0.001f)
#   endif
#endif

#ifndef MM2UM
#   ifdef KBD_MM2UM
#       define MM2UM(mm)    (KBD_MM2UM(mm))
#   else
#       define MM2UM(mm)    ((float)(mm) * 1000.0f)
#   endif
#endif

#define MAGIC_OBMAG 0x47414D5FU /* "_MAG" */
#define OBMAG_VER   0U

typedef union {
    uint32_t buf;
    struct { uint16_t top; uint16_t bot; };
    struct { uint16_t max; uint16_t min; };
} __attribute__((aligned(4))) keymag_lmt_t;

typedef struct {
    uint32_t magic;
    uint32_t crc32;
    uint32_t version;
    uint8_t  name[32];
    keymag_lmt_t lmt[KBD_KEY_MAX];
} __attribute__((aligned(4))) keymag_cal_t;

typedef struct {
    keymag_cal_t cal;
    keymag_cal_t cal_bak;
} obmag_t;

extern obmag_t g_obmag;

rotsta_t obmag_db_register(void);
rotsta_t keymag_init(void);
bool     keymag_lmt_valid(const keymag_lmt_t *p_lmt);
void     keymag_update_factor(uint8_t ki);
float    keymag_pt2du(uint8_t ki, uint16_t point);
uint16_t keymag_du2pt(uint8_t ki, float dist_um);


#endif  /* KEY_MAGNET_H */
