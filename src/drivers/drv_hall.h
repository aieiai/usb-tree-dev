#ifndef DRV_HALL_H
#define DRV_HALL_H

#include <stdint.h>
#include <stdbool.h>

#if defined(CHIP_SERIES_HPM5300)
#include "config/hall_scan_5300cfg.h"
#elif defined(CHIP_SERIES_HPM5100)
#include "config/hall_scan_5100cfg.h"
#endif

#define _INST_CONCAT(x, y) x##_##y
#define INST_CONCAT(x, y) _INST_CONCAT(x, y)
#define _INST_CONCAT3(x, y, z) x##_##y##_##z
#define INST_CONCAT3(x, y, z) _INST_CONCAT3(x, y, z)
#define _INST_RCONCAT(x, y) x##y
#define INST_RCONCAT(x, y) _INST_RCONCAT(x, y)
#define _INST_RCONCAT3(x, y, z) x##y##z
#define INST_RCONCAT3(x, y, z) _INST_RCONCAT3(x, y, z)

#define SENSOR_MATRIX_TMR_INST GPTMR1

//#define SENSOR_MATRIX_MUX_EN A31
//#define SENSOR_MATRIX_PWRL_PIN Y0
//#define SENSOR_MATRIX_PWRR_PIN Y1




/* MCHTMR 24MHz 换算：ns = ticks * 1e9 / 24e6 = ticks * 125 / 3；us = ticks / 24 */
#define SENSOR_MATRIX_MCHTMR_TICKS_TO_NS(t) ((uint32_t)(((uint64_t)(t)) * 125ULL / 3ULL))
#define SENSOR_MATRIX_MCHTMR_TICKS_TO_US(t) ((uint32_t)(((uint64_t)(t)) / 24ULL))
/* GPTMR 100MHz 换算：1 tick = 10ns */
#define SENSOR_MATRIX_TMR_TICKS_TO_NS(t)    ((uint32_t)(((uint64_t)(t)) * 10ULL))

#define SENSOR_TOTAL_SIZE                   (SENSOR_MATRIX_ROUND_SELS_MAX * SENSOR_MATRIX_ADC_TOTAL_CHANNELS)
#if ((SENSOR_MATRIX_ADC_SAMPLE_ROUNDS) == 1)
#    define GET_ADC_VALUE(p, i) ((uint16_t)((p)[(i)] & 0xFFFFU))
#else
#    define GET_ADC_VALUE(p, i) ((uint16_t)((((p)[(i) * 2U] & 0xFFFF) + ((p)[(i) * 2U + 1U] & 0xFFFF)) >> 1))
#endif


typedef void (*hall_recv_cb_t)(uint32_t *p_buf);

/*
 * 扫描时序测
 * isr_ts / frame_ticks 基于 HPM_CORE_MCYCLE，固定 core freq xMHz
 * sof_tick 基于 GPTMR（100MHz），1 tick = 10ns
 */
typedef struct {
    volatile uint64_t sof_start_tick;
    volatile uint64_t isr_tick;      /* 最近一帧扫描完成（DMA ISR）时的 MCYCLE 时间戳 */
    volatile uint64_t sof_tick;   /* SOF 沿 → 本帧扫描完成（GPTMR1 SHT 通道计数） */
    volatile uint32_t isr_count;   /* 已完成的扫描帧数 */
    
    volatile uint32_t scan_time_us;
    volatile uint32_t scan_period_us;

    /* 配置自检值，调试器里读取即可确认 SENSOR_MATRIX_ADC_SAMPLES 是否生效 */
    volatile uint16_t adc_seq_len;      /* 单路 ADC 最长序列长度 = 通道数 × 轮数 */
    volatile uint16_t adc_window_ticks; /* ADC 触发高电平宽度 = cmp[1] - cmp[0] */
    volatile uint16_t copy_words;       /* 每个 sel 的 DMA 拷贝 word 数 */
} sensor_matrix_timing_t;

extern volatile sensor_matrix_timing_t g_sensor_matrix_timing;

enum {
    sensor_matrix_sof_usb  = 0,
    sensor_matrix_sof_ext  = 1,
    sensor_matrix_sof_tmr  = 2,
    sensor_matrix_sof_none = 3,
};

typedef struct {
    bool valid_scan; /* change count and list */
    bool valid_mode;
    bool valid_timer;

    uint8_t  count;
    bool     mode;
    uint8_t  list[SENSOR_MATRIX_ROUND_SELS_MAX];
    uint32_t interval;
    float    adjust;
} sensor_matrix_set_config_t;


extern void sensor_matrix_init(void);
extern void sensor_matrix_recv_cb(hall_recv_cb_t recv_cb);
extern void sensor_matrix_set_sof_timer(uint32_t interval, float adjust);
extern void sensor_matrix_set_sof_src(uint8_t sof_src);
extern void sensor_matrix_restart_sof_timer(void);
extern void sensor_matrix_start_sof_timer(void);
extern void sensor_matrix_stop_sof_timer(void);
extern void sensor_matrix_set_config(sensor_matrix_set_config_t config);


#endif
