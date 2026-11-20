#ifndef DRV_HALL_H
#define DRV_HALL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>


#define _INST_CONCAT(x, y) x##_##y
#define INST_CONCAT(x, y) _INST_CONCAT(x, y)
#define _INST_CONCAT3(x, y, z) x##_##y##_##z
#define INST_CONCAT3(x, y, z) _INST_CONCAT3(x, y, z)
#define _INST_RCONCAT(x, y) x##y
#define INST_RCONCAT(x, y) _INST_RCONCAT(x, y)
#define _INST_RCONCAT3(x, y, z) x##y##z
#define INST_RCONCAT3(x, y, z) _INST_RCONCAT3(x, y, z)



#if !defined(CEILING)
/**
 * @brief Computes the rounded up result of a division of two integers at
 * compile time.
 */
#    define CEILING(dividend, divisor) (((dividend) + (divisor)-1) / (divisor))
#endif

#define SENSOR_MATRIX_ADC_SAMPLES 2        /* sample 1 in once sel */
#define SENSOR_MATRIX_ADC_SAMPLE_ROUNDS (((SENSOR_MATRIX_ADC_SAMPLES) > 1) ? 2 : 1)
#define SENSOR_MATRIX_ADC_SAMPLE_CYCLE 2   /* adc sample cycle */
#define SENSOR_MATRIX_ADC_CONVERT_CYCLE 21 /* 16-bit adc need 21 convert cycle */

#define SENSOR_MATRIX_ROUND_SELS_MAX 8 /* scan 40 sel in once shot */

#define SENSOR_MATRIX_TMR_SHOT_FREQ 8000  /* scan shot freq */
#define SENSOR_MATRIX_TMR_SEL_FREQ 250000 /* sel freq，与 50MHz ADC + 2 次采样配套 */

#define SENSOR_MATRIX_SOF_ST_TIME 2000 /* (sof to tmr irq margin time) */
#define SENSOR_MATRIX_TMR_ST_TIME 70 /* tmr soft sync setup time default 350ns, DMA 握手，写 GPIO 切 mux  */
#define SENSOR_MATRIX_SEL_HT_TIME1 1 
#define SENSOR_MATRIX_SEL_HT_TIME2 45 // sel 保持时间
#define SENSOR_MATRIX_ADC_ST_TIME 100 /* sample setup time is 1.0us,  模拟通路建立 */


#define SENSOR_MATRIX_TMR_INST GPTMR1

#define SENSOR_MATRIX_ADDRS 3
#define SENSOR_MATRIX_ADDR_PINS {C2, C3, C4}

//#define SENSOR_MATRIX_MUX_EN A31
//#define SENSOR_MATRIX_PWRL_PIN Y0
//#define SENSOR_MATRIX_PWRR_PIN Y1

/* COL2ROW COL is ADC, ROW is SEL */
#define SENSOR_MATRIX_SELS 8
#define SENSOR_MATRIX_SELS_SEQUENCE {0, 1, 2, 3, 4, 5, 6, 7}


#define SENSOR_MATRIX_ANAS 10
#define SENSOR_MATRIX_ANA_PINS \
    {                   \
        C5,  C6,  C7,   \
        A12, A13,       \
        B4,  B5,  B6,   \
        C11, C12,       \
    }
#define SENSOR_MATRIX_ANA_FUNC          \
    {                                   \
        IOC_ANALOG(C5,  ADC0, CH2),     \
        IOC_ANALOG(C6,  ADC0, CH1),     \
        IOC_ANALOG(C7,  ADC0, CH15),    \
                                        \
        IOC_ANALOG(A12, ADC1, CH9),     \
        IOC_ANALOG(A13, ADC1, CH13),    \
                                        \
        IOC_ANALOG(B4,  ADC2, CH12),    \
        IOC_ANALOG(B5,  ADC2, CH8),     \
        IOC_ANALOG(B6,  ADC2, CH0),     \
                                        \
        IOC_ANALOG(C11, ADC3, CH4),     \
        IOC_ANALOG(C12, ADC3, CH3),     \
    }

#define SENSOR_MATRIX_ADC_INSTANCES 4
#define SENSOR_MATRIX_ADC_CHANNELS CEILING(SENSOR_MATRIX_ANAS, SENSOR_MATRIX_ADC_INSTANCES)
#define SENSOR_MATRIX_ADC_TOTAL_CHANNELS (SENSOR_MATRIX_ANAS) /* 4 路 ADC 实际使用的通道总数（等于模拟引脚数），扫描缓冲按这个数量紧密排列 */

/* Rows follow the physical ADC order in SENSOR_MATRIX_ANA_PINS. */
#define SENSOR_MATRIX_ADC_CHANNEL_COUNTS {3, 2, 3, 2}


/* MCHTMR 24MHz 换算：ns = ticks * 1e9 / 24e6 = ticks * 125 / 3；us = ticks / 24 */
#define SENSOR_MATRIX_MCHTMR_TICKS_TO_NS(t) ((uint32_t)(((uint64_t)(t)) * 125ULL / 3ULL))
#define SENSOR_MATRIX_MCHTMR_TICKS_TO_US(t) ((uint32_t)(((uint64_t)(t)) / 24ULL))
/* GPTMR 200MHz 换算：1 tick = 5ns */
#define SENSOR_MATRIX_TMR_TICKS_TO_NS(t)    ((uint32_t)(((uint64_t)(t)) * 5ULL))

#if ((SENSOR_MATRIX_ADC_SAMPLE_ROUNDS) == 1)
#    define GET_ADC_VALUE(p, i) ((uint16_t)((p)[(i)] & 0xFFFFU))
#else
#    define GET_ADC_VALUE(p, i) ((uint16_t)((((p)[(i) * 2U] & 0xFFFF) + ((p)[(i) * 2U + 1U] & 0xFFFF)) >> 1))
#endif


typedef void (*hall_recv_cb_t)(uint32_t *p_buf);

/*
 * 扫描时序测
 * isr_ts / frame_ticks 基于 HPM_CORE_MCYCLE，固定 core freq xMHz
 * sof_tick 基于 GPTMR（200MHz），1 tick = 5ns
 */
typedef struct {
    volatile uint64_t isr_tick;      /* 最近一帧扫描完成（DMA ISR）时的 MCHTMR 时间戳 */
    volatile uint64_t sof_start_tick;

    volatile uint32_t sof_tick;   /* SOF 沿 → 本帧扫描完成（GPTMR1 SHT 通道计数） */
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

/*
 * One raw ADC sample from the scan matrix.
 * Override this weak function in the application to feed the keyboard's
 * logical key mapping.
 */
extern void sensor_matrix_on_sample(uint8_t row, uint8_t col, uint16_t value);

extern void sensor_matrix_init(hall_recv_cb_t recv_cb);
extern void sensor_matrix_set_sof_timer(uint32_t interval, float adjust);
extern void sensor_matrix_set_sof_src(uint8_t sof_src);
extern void sensor_matrix_restart_sof_timer(void);
extern void sensor_matrix_start_sof_timer(void);
extern void sensor_matrix_stop_sof_timer(void);
extern void sensor_matrix_set_config(sensor_matrix_set_config_t config);


#endif
