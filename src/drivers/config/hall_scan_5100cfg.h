#ifndef HALL_SCAN_5100CFG_H
#define HALL_SCAN_5100CFG_H


#ifndef SENSOR_MATRIX_SOF_EXT
#    define SENSOR_MATRIX_SOF_EXT INST_CONCAT(TRGM0, P00)
#endif



#if !defined(CEILING)
/**
 * @brief Computes the rounded up result of a division of two integers at
 * compile time.
 */
#    define CEILING(dividend, divisor) (((dividend) + (divisor)-1) / (divisor))
#endif

#define SENSOR_MATRIX_ADC_SAMPLES 2        /* sample 2 in once sel */
#define SENSOR_MATRIX_ADC_SAMPLE_ROUNDS (((SENSOR_MATRIX_ADC_SAMPLES) > 1) ? 2 : 1)
#define SENSOR_MATRIX_ADC_SAMPLE_CYCLE 2   /* adc sample cycle */
#define SENSOR_MATRIX_ADC_CONVERT_CYCLE 21 /* 16-bit adc need 21 convert cycle */
#define SENSOR_MATRIX_ADC_DIV 4


#define SENSOR_MATRIX_TMR_SHOT_FREQ 8000  /* scan shot freq */
//#define SENSOR_MATRIX_TMR_SEL_FREQ 218800 /* 100MHz / 218800 = 457 ticks (4.57us)，单周期留 0.10us 余量 */

#define SENSOR_MATRIX_SOF_ST_TIME 2000 /* (sof to tmr irq margin time), 20.0us @100MHz */
#define SENSOR_MATRIX_TMR_ST_TIME 70   /* tmr soft sync setup time 700ns @100MHz, DMA 握手写 GPIO 切 mux */
#define SENSOR_MATRIX_SEL_HT_TIME1 1 /* MUX切换完成点 */
#define SENSOR_MATRIX_SEL_HT_TIME2 45  /* sel 保持时间 */
#define SENSOR_MATRIX_ADC_ST_TIME 100  /* sample setup time is 1.0us @100MHz, 模拟通路建立 */



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

#define SENSOR_MATRIX_ADC_PORT {HPM_ADC0, HPM_ADC1, HPM_ADC2, HPM_ADC3}

#define SENSOR_MATRIX_ADC_OUTPUT_SOURCES    \
    {                                       \
        HPM_TRGM0_OUTPUT_SRC_ADC0_STRGI,    \
        HPM_TRGM0_OUTPUT_SRC_ADC1_STRGI,    \
        HPM_TRGM0_OUTPUT_SRC_ADC2_STRGI,    \
        HPM_TRGM0_OUTPUT_SRC_ADC3_STRGI,    \
    }

#define SENSOR_MATRIX_ADDRS 3
#define SENSOR_MATRIX_ADDR_PINS {C2, C3, C4}

/* COL2ROW COL is ADC, ROW is SEL */
#define SENSOR_MATRIX_SELS 8
#define SENSOR_MATRIX_ROUND_SELS_MAX 8 /* scan 8 sel in once shot, SENSOR_MATRIX_SELS_SEQUENCE */
// {0, 3, 6, 1, 4, 7, 2, 5}
#define SENSOR_MATRIX_SELS_SEQUENCE {0, 1, 2, 3, 4, 5, 6, 7}


/* ==================== 自动计算 SEL 扫描频率 ==================== */
#define SENSOR_MATRIX_TMR_CLK_HZ        100000000ULL /* GPTMR1 时钟: 100MHz */
#define SENSOR_MATRIX_ADC_CLK_HZ        50000000ULL  /* ADC 工作时钟: 50MHz */
#define SENSOR_MATRIX_SEL_MARGIN_TICKS  10ULL        /* 单周期尾部余量: 10 ticks = 0.10us @100MHz */

/* 1. 单路最长 ADC 序列转换所需 GPTMR ticks（使用 ULL 防止 32 位平台乘法溢出） */
#define SENSOR_MATRIX_ADC_CONV_TICKS                                                                            \
    CEILING((SENSOR_MATRIX_TMR_CLK_HZ) * ((SENSOR_MATRIX_ADC_SAMPLE_CYCLE) + (SENSOR_MATRIX_ADC_CONVERT_CYCLE)) \
                * (SENSOR_MATRIX_ADC_CHANNELS) * (SENSOR_MATRIX_ADC_SAMPLE_ROUNDS),                             \
            (SENSOR_MATRIX_ADC_CLK_HZ))

/* 2. 单路 SEL 所需最小 reload ticks = 70 + 1 + ADC_ST + ADC_CONV + MARGIN */
#define SENSOR_MATRIX_SEL_RELOAD_TICKS                                                                          \
    ((SENSOR_MATRIX_TMR_ST_TIME) + (SENSOR_MATRIX_SEL_HT_TIME1) + (SENSOR_MATRIX_ADC_ST_TIME)                   \
     + (SENSOR_MATRIX_ADC_CONV_TICKS) + (SENSOR_MATRIX_SEL_MARGIN_TICKS))

/* 3. 自动推导出 SENSOR_MATRIX_TMR_SEL_FREQ（向下取整，保证反算回 reload >= 目标值） */
#define SENSOR_MATRIX_TMR_SEL_FREQ      ((SENSOR_MATRIX_TMR_CLK_HZ) / (SENSOR_MATRIX_SEL_RELOAD_TICKS))

/* 4. 编译期防错：确保整帧（含 SOF 延时 + 8 路扫描 + 至少 20us 处理器余量）不超过 8kHz 周期 */
#if ((10ULL + (SENSOR_MATRIX_SOF_ST_TIME) + (SENSOR_MATRIX_ROUND_SELS_MAX) * (SENSOR_MATRIX_SEL_RELOAD_TICKS)   \
      + (SENSOR_MATRIX_TMR_ST_TIME) + 2000ULL) >= ((SENSOR_MATRIX_TMR_CLK_HZ) / (SENSOR_MATRIX_TMR_SHOT_FREQ)))
#    error "Hall scan total time exceeds 8kHz frame budget! Reduce ADC_ST_TIME / SOF_ST_TIME / MARGIN."
#endif

#endif
