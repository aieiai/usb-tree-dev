#ifndef KEYCALIB_H
#define KEYCALIB_H

#include <stdbool.h>
#include <stdint.h>
#include "rotsta.h"
#include "kbd_config.h"
#include "keymagnet.h"


/* Reported by the Rot-CC calibration status command. */
enum {
    KCAL_STA_STOP = 0x80,
    KCAL_STA_START,
    KCAL_STA_FINISH,
};

typedef struct {
    keymag_lmt_t acc[KBD_KEY_MAX];
    uint16_t *frame;
    uint8_t sta;
} __attribute__((aligned(4))) keycal_sta_t;

extern keycal_sta_t g_keycal_sta;

rotsta_t keycal_init(void);

/* 0x81 starts sampling, 0x80 stops and commits the collected range. */
rotsta_t keycal_set_status(uint8_t sta);

/* Core thread copies one ADC frame per key and notifies the calibration task. */
void keycal_post_adc(uint16_t *p_buf);

#define _keycal_get_status()    (g_keycal_sta.sta)

/* True while the calibration thread is collecting samples. */
#define _keycal_active()        (g_keycal_sta.sta == KCAL_STA_START)


#endif  /* KEYCALIB_H */
