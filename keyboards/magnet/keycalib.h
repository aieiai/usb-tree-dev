#ifndef KEYCALIB_H
#define KEYCALIB_H

#include <stdbool.h>
#include <stdint.h>
#include "rotsta.h"
#include "kbd_config.h"


/* Reported by the Rot-CC calibration status command. */
enum {
    KCALIB_STA_STOP = 0x80,
    KCALIB_STA_START,
    KCALIB_STA_FINISH,
};


rotsta_t keycal_init(void);

uint8_t keycalib_get_status(void);

/* 0x81 starts sampling, 0x80 stops and commits the collected range. */
rotsta_t keycalib_set_status(uint8_t sta);

/* True while the calibration thread is collecting samples. */
bool keycalib_active(void);

/* Core thread copies one ADC frame per key and notifies the calibration task. */
void keycalib_post_adc(uint16_t *adc);


#endif  /* KEYCALIB_H */
