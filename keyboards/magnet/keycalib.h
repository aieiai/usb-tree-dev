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

#define KCALIB_SENSOR_FREQ_DFT  (50U)
#define KCALIB_SENSOR_FREQ_MIN  (1U)
#define KCALIB_SENSOR_FREQ_MAX  (1000U)

typedef struct {
    keymag_lmt_t acc[KBD_KEY_MAX];
    uint16_t *frame;
    uint8_t sta;
    volatile uint8_t  rpt_en;   /* sensor data upload switch */
    volatile uint16_t rpt_freq; /* unit: Hz */
    volatile uint32_t rpt_last; /* tick of the last report period */
} __attribute__((aligned(4))) keycal_sta_t;

extern keycal_sta_t g_keycal_sta;

rotsta_t keycal_init(void);

/* 0x81 starts sampling, 0x80 stops and commits the collected range. */
rotsta_t keycal_set_status(uint8_t sta);

/* Sensor data upload, independent of the manual calibration session. */
rotsta_t keycal_sensor_report_set(uint8_t en);
rotsta_t keycal_sensor_freq_set(uint16_t freq);

/* Core thread copies one ADC frame per key and notifies the calibration task. */
void keycal_post_adc(uint16_t *p_buf);

#define _keycal_get_status()    (g_keycal_sta.sta)

/* True while the calibration thread is collecting samples. */
#define _keycal_active()        (g_keycal_sta.sta == KCAL_STA_START)


#endif  /* KEYCALIB_H */
