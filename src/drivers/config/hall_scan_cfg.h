#ifndef HALL_SCAN_CFG_H
#define HALL_SCAN_CFG_H


#if defined(CHIP_SERIES_HPM5300)

#ifndef SENSOR_MATRIX_SOF_EXT
#    define SENSOR_MATRIX_SOF_EXT INST_CONCAT(TRGM0, P0)
#endif

#define SENSOR_MATRIX_ANAS 10
#define SENSOR_MATRIX_ANA_PINS      \
    {                               \
        B9,  B10, B11, B12, B13,    \
        B0,  B1,  B2,  B3,  B4,     \
    }
#define SENSOR_MATRIX_ANA_FUNC          \
    {                                   \
        IOC_ANALOG(B9,  ADC0, CH1),     \
        IOC_ANALOG(B10, ADC0, CH2),     \
        IOC_ANALOG(B11, ADC0, CH3),     \
        IOC_ANALOG(B12, ADC0, CH4),     \
        IOC_ANALOG(B13, ADC0, CH5),     \
                                        \
        IOC_ANALOG(B0,  ADC1, CH15),    \
        IOC_ANALOG(B1,  ADC1, CH14),    \
        IOC_ANALOG(B2,  ADC1, CH12),    \
        IOC_ANALOG(B3,  ADC1, CH8),     \
        IOC_ANALOG(B4,  ADC1, CH0),     \
    }

#define SENSOR_MATRIX_ADC_INSTANCES 2
#define SENSOR_MATRIX_ADC_CHANNELS CEILING(SENSOR_MATRIX_ANAS, SENSOR_MATRIX_ADC_INSTANCES)
#define SENSOR_MATRIX_ADC_TOTAL_CHANNELS (SENSOR_MATRIX_ANAS) /* 2 路 ADC 实际使用的通道总数（等于模拟引脚数），扫描缓冲按这个数量紧密排列 */

/* Rows follow the physical ADC order in SENSOR_MATRIX_ANA_PINS. */
#define SENSOR_MATRIX_ADC_CHANNEL_COUNTS {5, 5}

#define SENSOR_MATRIX_ADC_PORT {HPM_ADC0, HPM_ADC1}

#define SENSOR_MATRIX_ADC_OUTPUT_SOURCES    \
    {                                       \
        HPM_TRGM0_OUTPUT_SRC_ADC0_STRGI,    \
        HPM_TRGM0_OUTPUT_SRC_ADC1_STRGI,    \
    }

#define SENSOR_MATRIX_ADDRS 3
#define SENSOR_MATRIX_ADDR_PINS {A0, A2, A1}

/* COL2ROW COL is ADC, ROW is SEL */
#define SENSOR_MATRIX_SELS 8
#define SENSOR_MATRIX_SELS_SEQUENCE {0, 1, 2, 3, 4, 5, 6, 7}



#elif defined(CHIP_SERIES_HPM5100)

#ifndef SENSOR_MATRIX_SOF_EXT
#    define SENSOR_MATRIX_SOF_EXT INST_CONCAT(TRGM0, P00)
#endif

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
#define SENSOR_MATRIX_SELS_SEQUENCE {0, 1, 2, 3, 4, 5, 6, 7}

#else
#error "Unsupported HPM series"
#endif



#endif
