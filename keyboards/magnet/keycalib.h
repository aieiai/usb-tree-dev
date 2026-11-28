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
#define KCALIB_SENSOR_FREQ_MAX  (8000U)
#define KCALIB_SENSOR_FRAME_HZ  (8000U)

typedef struct {
    keymag_lmt_t acc[KBD_KEY_MAX];
    uint16_t *frame;
    uint8_t sta;
    volatile uint8_t  busy;     /* 1: 校准或 sensor 上报至少有一个开启 */
    volatile uint8_t  rpt_en;   /* sensor data upload switch */
    volatile uint16_t rpt_freq; /* unit: Hz */
    volatile uint16_t rpt_div;  /* frames per report: ceil(8000 / rpt_freq) */
    volatile uint16_t rpt_cnt;  /* frame counter inside the current period */
} __attribute__((aligned(4))) keycal_sta_t;

extern keycal_sta_t g_keycal_sta;

rotsta_t keycal_init(void);

/* 0x81 starts sampling, 0x80 stops and commits the collected range. */
rotsta_t keycal_set_status(uint8_t sta);

/* Sensor data upload, independent of the manual calibration session. */
rotsta_t keycal_sensor_report_set(uint8_t en);
rotsta_t keycal_sensor_freq_set(uint16_t freq);

/* Core thread copies one data frame per key and notifies the calibration task. */
void keycal_post_data(uint16_t *p_buf);

#define _keycal_get_state()     (g_keycal_sta.sta)

/* True while the calibration thread is collecting samples. */
#define _keycal_active()        (g_keycal_sta.sta == KCAL_STA_START)

/* True while calibration or sensor upload needs data frames. */
#define _keycal_busy()          (g_keycal_sta.busy != 0U)


#endif  /* KEYCALIB_H */
