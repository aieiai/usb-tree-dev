#ifndef HALL_SCAN_5300CFG_H
#define HALL_SCAN_5300CFG_H


#ifndef SENSOR_MATRIX_SOF_EXT
#    define SENSOR_MATRIX_SOF_EXT INST_CONCAT(TRGM0, P0)
#endif


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


#define SENSOR_MATRIX_TMR_SHOT_FREQ 8000  /* scan shot freq */
#define SENSOR_MATRIX_TMR_SEL_FREQ 150000 /* sel freq，与 50MHz ADC + SENSOR_MATRIX_ADC_SAMPLES 次采样配套 */

#define SENSOR_MATRIX_SOF_ST_TIME 3500 /* (sof to tmr irq margin time) */
#define SENSOR_MATRIX_TMR_ST_TIME 70 /* tmr soft sync setup time default 350ns, DMA 握手，写 GPIO 切 mux  */
#define SENSOR_MATRIX_SEL_HT_TIME1 1 
#define SENSOR_MATRIX_SEL_HT_TIME2 45 // sel 保持时间
#define SENSOR_MATRIX_ADC_ST_TIME 100 /* sample setup time is 1.0us,  模拟通路建立 */




#define SENSOR_MATRIX_ANAS 12
#define SENSOR_MATRIX_ANA_PINS      \
    {                               \
        B0, B1, B2, B3, B4,  B5,    \
        B6, B7, B8, B9, B10, B11,   \
    }
#define SENSOR_MATRIX_ANA_FUNC          \
    {                                   \
        IOC_ANALOG(B5,  ADC0, CH13),    \
        IOC_ANALOG(B4,  ADC0, CH0),     \
        IOC_ANALOG(B1,  ADC0, CH14),    \
        IOC_ANALOG(B3,  ADC0, CH8),     \
        IOC_ANALOG(B0,  ADC0, CH15),    \
        IOC_ANALOG(B2,  ADC0, CH12),    \
                                        \
        IOC_ANALOG(B11, ADC1, CH3),     \
        IOC_ANALOG(B10, ADC1, CH2),     \
        IOC_ANALOG(B7,  ADC1, CH10),    \
        IOC_ANALOG(B9,  ADC1, CH1),     \
        IOC_ANALOG(B6,  ADC1, CH9),     \
        IOC_ANALOG(B8,  ADC1, CH11),    \
    }

#define SENSOR_MATRIX_ADC_INSTANCES 2
#define SENSOR_MATRIX_ADC_CHANNELS CEILING(SENSOR_MATRIX_ANAS, SENSOR_MATRIX_ADC_INSTANCES)
#define SENSOR_MATRIX_ADC_TOTAL_CHANNELS (SENSOR_MATRIX_ANAS) /* 2 路 ADC 实际使用的通道总数（等于模拟引脚数），扫描缓冲按这个数量紧密排列 */

/* Rows follow the physical ADC order in SENSOR_MATRIX_ANA_PINS. */
#define SENSOR_MATRIX_ADC_CHANNEL_COUNTS {6, 6}

#define SENSOR_MATRIX_ADC_PORT {HPM_ADC0, HPM_ADC1}

#define SENSOR_MATRIX_ADC_OUTPUT_SOURCES    \
    {                                       \
        HPM_TRGM0_OUTPUT_SRC_ADC0_STRGI,    \
        HPM_TRGM0_OUTPUT_SRC_ADC1_STRGI,    \
    }

#define SENSOR_MATRIX_ADDRS 3
#define SENSOR_MATRIX_ADDR_PINS {A19, A20, A21}

/* COL2ROW COL is ADC, ROW is SEL */
#define SENSOR_MATRIX_SELS 8
#define SENSOR_MATRIX_ROUND_SELS_MAX 6 /* scan 40 sel in once shot, SENSOR_MATRIX_SELS_SEQUENCE */
#define SENSOR_MATRIX_SELS_SEQUENCE  {2, 5, 1, 7, 0, 3}


#endif
