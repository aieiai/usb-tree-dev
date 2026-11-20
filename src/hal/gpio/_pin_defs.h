/*
 * Copyright 2025 HPMicro
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "hpm_soc.h"

typedef struct {
    IOC_Type  *ioc;
    GPIO_Type *ngpio;
    uint16_t   npad;
    uint8_t    nport;
    uint8_t    npin;
} pin_t;

#define IOC_ANALOG_FUNC_CTL_ADC0_CH0 0x00
#define IOC_ANALOG_FUNC_CTL_ADC0_CH1 0x01
#define IOC_ANALOG_FUNC_CTL_ADC0_CH2 0x02
#define IOC_ANALOG_FUNC_CTL_ADC0_CH3 0x03
#define IOC_ANALOG_FUNC_CTL_ADC0_CH4 0x04
#define IOC_ANALOG_FUNC_CTL_ADC0_CH5 0x05
#define IOC_ANALOG_FUNC_CTL_ADC0_CH6 0x06
#define IOC_ANALOG_FUNC_CTL_ADC0_CH7 0x07
#define IOC_ANALOG_FUNC_CTL_ADC0_CH8 0x08
#define IOC_ANALOG_FUNC_CTL_ADC0_CH9 0x09
#define IOC_ANALOG_FUNC_CTL_ADC0_CH10 0x0A
#define IOC_ANALOG_FUNC_CTL_ADC0_CH11 0x0B
#define IOC_ANALOG_FUNC_CTL_ADC0_CH12 0x0C
#define IOC_ANALOG_FUNC_CTL_ADC0_CH13 0x0D
#define IOC_ANALOG_FUNC_CTL_ADC0_CH14 0x0E
#define IOC_ANALOG_FUNC_CTL_ADC0_CH15 0x0F

#define IOC_ANALOG_FUNC_CTL_ADC1_CH0 0x10
#define IOC_ANALOG_FUNC_CTL_ADC1_CH1 0x11
#define IOC_ANALOG_FUNC_CTL_ADC1_CH2 0x12
#define IOC_ANALOG_FUNC_CTL_ADC1_CH3 0x13
#define IOC_ANALOG_FUNC_CTL_ADC1_CH4 0x14
#define IOC_ANALOG_FUNC_CTL_ADC1_CH5 0x15
#define IOC_ANALOG_FUNC_CTL_ADC1_CH6 0x16
#define IOC_ANALOG_FUNC_CTL_ADC1_CH7 0x17
#define IOC_ANALOG_FUNC_CTL_ADC1_CH8 0x18
#define IOC_ANALOG_FUNC_CTL_ADC1_CH9 0x19
#define IOC_ANALOG_FUNC_CTL_ADC1_CH10 0x1A
#define IOC_ANALOG_FUNC_CTL_ADC1_CH11 0x1B
#define IOC_ANALOG_FUNC_CTL_ADC1_CH12 0x1C
#define IOC_ANALOG_FUNC_CTL_ADC1_CH13 0x1D
#define IOC_ANALOG_FUNC_CTL_ADC1_CH14 0x1E
#define IOC_ANALOG_FUNC_CTL_ADC1_CH15 0x1F

#define IOC_ANALOG_FUNC_CTL_ADC2_CH0 0x20
#define IOC_ANALOG_FUNC_CTL_ADC2_CH1 0x21
#define IOC_ANALOG_FUNC_CTL_ADC2_CH2 0x22
#define IOC_ANALOG_FUNC_CTL_ADC2_CH3 0x23
#define IOC_ANALOG_FUNC_CTL_ADC2_CH4 0x24
#define IOC_ANALOG_FUNC_CTL_ADC2_CH5 0x25
#define IOC_ANALOG_FUNC_CTL_ADC2_CH6 0x26
#define IOC_ANALOG_FUNC_CTL_ADC2_CH7 0x27
#define IOC_ANALOG_FUNC_CTL_ADC2_CH8 0x28
#define IOC_ANALOG_FUNC_CTL_ADC2_CH9 0x29
#define IOC_ANALOG_FUNC_CTL_ADC2_CH10 0x2A
#define IOC_ANALOG_FUNC_CTL_ADC2_CH11 0x2B
#define IOC_ANALOG_FUNC_CTL_ADC2_CH12 0x2C
#define IOC_ANALOG_FUNC_CTL_ADC2_CH13 0x2D
#define IOC_ANALOG_FUNC_CTL_ADC2_CH14 0x2E
#define IOC_ANALOG_FUNC_CTL_ADC2_CH15 0x2F

#define IOC_ANALOG_FUNC_CTL_ADC3_CH0 0x30
#define IOC_ANALOG_FUNC_CTL_ADC3_CH1 0x31
#define IOC_ANALOG_FUNC_CTL_ADC3_CH2 0x32
#define IOC_ANALOG_FUNC_CTL_ADC3_CH3 0x33
#define IOC_ANALOG_FUNC_CTL_ADC3_CH4 0x34
#define IOC_ANALOG_FUNC_CTL_ADC3_CH5 0x35
#define IOC_ANALOG_FUNC_CTL_ADC3_CH6 0x36
#define IOC_ANALOG_FUNC_CTL_ADC3_CH7 0x37
#define IOC_ANALOG_FUNC_CTL_ADC3_CH8 0x38
#define IOC_ANALOG_FUNC_CTL_ADC3_CH9 0x39
#define IOC_ANALOG_FUNC_CTL_ADC3_CH10 0x3A
#define IOC_ANALOG_FUNC_CTL_ADC3_CH11 0x3B
#define IOC_ANALOG_FUNC_CTL_ADC3_CH12 0x3C
#define IOC_ANALOG_FUNC_CTL_ADC3_CH13 0x3D
#define IOC_ANALOG_FUNC_CTL_ADC3_CH14 0x3E
#define IOC_ANALOG_FUNC_CTL_ADC3_CH15 0x3F

/* Include the vendor specific pin defs */
#define A0 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA00, .nport = GPIO_DI_GPIOA, .npin = 0})
#define A1 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA01, .nport = GPIO_DI_GPIOA, .npin = 1})
#define A2 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA02, .nport = GPIO_DI_GPIOA, .npin = 2})
#define A3 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA03, .nport = GPIO_DI_GPIOA, .npin = 3})
#define A4 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA04, .nport = GPIO_DI_GPIOA, .npin = 4})
#define A5 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA05, .nport = GPIO_DI_GPIOA, .npin = 5})
#define A6 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA06, .nport = GPIO_DI_GPIOA, .npin = 6})
#define A7 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA07, .nport = GPIO_DI_GPIOA, .npin = 7})
#define A8 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA08, .nport = GPIO_DI_GPIOA, .npin = 8})
#define A9 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA09, .nport = GPIO_DI_GPIOA, .npin = 9})
#define A10 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA10, .nport = GPIO_DI_GPIOA, .npin = 10})
#define A11 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA11, .nport = GPIO_DI_GPIOA, .npin = 11})
#define A12 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA12, .nport = GPIO_DI_GPIOA, .npin = 12})
#define A13 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA13, .nport = GPIO_DI_GPIOA, .npin = 13})
#define A14 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA14, .nport = GPIO_DI_GPIOA, .npin = 14})
#define A15 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA15, .nport = GPIO_DI_GPIOA, .npin = 15})
#define A16 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA16, .nport = GPIO_DI_GPIOA, .npin = 16})
#define A17 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA17, .nport = GPIO_DI_GPIOA, .npin = 17})
#define A18 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA18, .nport = GPIO_DI_GPIOA, .npin = 18})
#define A19 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA19, .nport = GPIO_DI_GPIOA, .npin = 19})
#define A20 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA20, .nport = GPIO_DI_GPIOA, .npin = 20})
#define A21 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA21, .nport = GPIO_DI_GPIOA, .npin = 21})
#define A22 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA22, .nport = GPIO_DI_GPIOA, .npin = 22})
#define A23 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA23, .nport = GPIO_DI_GPIOA, .npin = 23})
#define A24 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA24, .nport = GPIO_DI_GPIOA, .npin = 24})
#define A25 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA25, .nport = GPIO_DI_GPIOA, .npin = 25})
#define A26 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA26, .nport = GPIO_DI_GPIOA, .npin = 26})
#define A27 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA27, .nport = GPIO_DI_GPIOA, .npin = 27})
#define A28 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA28, .nport = GPIO_DI_GPIOA, .npin = 28})
#define A29 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA29, .nport = GPIO_DI_GPIOA, .npin = 29})
#define A30 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA30, .nport = GPIO_DI_GPIOA, .npin = 30})
#define A31 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PA31, .nport = GPIO_DI_GPIOA, .npin = 31})

#define B0 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB00, .nport = GPIO_DI_GPIOB, .npin = 0})
#define B1 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB01, .nport = GPIO_DI_GPIOB, .npin = 1})
#define B2 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB02, .nport = GPIO_DI_GPIOB, .npin = 2})
#define B3 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB03, .nport = GPIO_DI_GPIOB, .npin = 3})
#define B4 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB04, .nport = GPIO_DI_GPIOB, .npin = 4})
#define B5 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB05, .nport = GPIO_DI_GPIOB, .npin = 5})
#define B6 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB06, .nport = GPIO_DI_GPIOB, .npin = 6})
#define B7 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB07, .nport = GPIO_DI_GPIOB, .npin = 7})
#define B8 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB08, .nport = GPIO_DI_GPIOB, .npin = 8})
#define B9 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB09, .nport = GPIO_DI_GPIOB, .npin = 9})
#define B10 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB10, .nport = GPIO_DI_GPIOB, .npin = 10})
#define B11 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB11, .nport = GPIO_DI_GPIOB, .npin = 11})
#define B12 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB12, .nport = GPIO_DI_GPIOB, .npin = 12})
#define B13 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB13, .nport = GPIO_DI_GPIOB, .npin = 13})
#define B14 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB14, .nport = GPIO_DI_GPIOB, .npin = 14})
#define B15 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB15, .nport = GPIO_DI_GPIOB, .npin = 15})
#define B16 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB16, .nport = GPIO_DI_GPIOB, .npin = 16})
#define B17 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB17, .nport = GPIO_DI_GPIOB, .npin = 17})
#define B18 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB18, .nport = GPIO_DI_GPIOB, .npin = 18})
#define B19 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB19, .nport = GPIO_DI_GPIOB, .npin = 19})
#define B20 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB20, .nport = GPIO_DI_GPIOB, .npin = 20})
#define B21 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB21, .nport = GPIO_DI_GPIOB, .npin = 21})
#define B22 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB22, .nport = GPIO_DI_GPIOB, .npin = 22})
#define B23 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB23, .nport = GPIO_DI_GPIOB, .npin = 23})
#define B24 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB24, .nport = GPIO_DI_GPIOB, .npin = 24})
#define B25 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB25, .nport = GPIO_DI_GPIOB, .npin = 25})
#define B26 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB26, .nport = GPIO_DI_GPIOB, .npin = 26})
#define B27 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB27, .nport = GPIO_DI_GPIOB, .npin = 27})
#define B28 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB28, .nport = GPIO_DI_GPIOB, .npin = 28})
#define B29 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB29, .nport = GPIO_DI_GPIOB, .npin = 29})
#define B30 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB30, .nport = GPIO_DI_GPIOB, .npin = 30})
#define B31 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PB31, .nport = GPIO_DI_GPIOB, .npin = 31})

#define C0 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC00, .nport = GPIO_DI_GPIOC, .npin = 0})
#define C1 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC01, .nport = GPIO_DI_GPIOC, .npin = 1})
#define C2 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC02, .nport = GPIO_DI_GPIOC, .npin = 2})
#define C3 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC03, .nport = GPIO_DI_GPIOC, .npin = 3})
#define C4 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC04, .nport = GPIO_DI_GPIOC, .npin = 4})
#define C5 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC05, .nport = GPIO_DI_GPIOC, .npin = 5})
#define C6 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC06, .nport = GPIO_DI_GPIOC, .npin = 6})
#define C7 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC07, .nport = GPIO_DI_GPIOC, .npin = 7})
#define C8 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC08, .nport = GPIO_DI_GPIOC, .npin = 8})
#define C9 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC09, .nport = GPIO_DI_GPIOC, .npin = 9})
#define C10 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC10, .nport = GPIO_DI_GPIOC, .npin = 10})
#define C11 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC11, .nport = GPIO_DI_GPIOC, .npin = 11})
#define C12 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC12, .nport = GPIO_DI_GPIOC, .npin = 12})
#define C13 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC13, .nport = GPIO_DI_GPIOC, .npin = 13})
#define C14 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC14, .nport = GPIO_DI_GPIOC, .npin = 14})
#define C15 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC15, .nport = GPIO_DI_GPIOC, .npin = 15})
#define C16 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC16, .nport = GPIO_DI_GPIOC, .npin = 16})
#define C17 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC17, .nport = GPIO_DI_GPIOC, .npin = 17})
#define C18 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC18, .nport = GPIO_DI_GPIOC, .npin = 18})
#define C19 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC19, .nport = GPIO_DI_GPIOC, .npin = 19})
#define C20 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC20, .nport = GPIO_DI_GPIOC, .npin = 20})
#define C21 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC21, .nport = GPIO_DI_GPIOC, .npin = 21})
#define C22 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC22, .nport = GPIO_DI_GPIOC, .npin = 22})
#define C23 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC23, .nport = GPIO_DI_GPIOC, .npin = 23})
#define C24 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC24, .nport = GPIO_DI_GPIOC, .npin = 24})
#define C25 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC25, .nport = GPIO_DI_GPIOC, .npin = 25})
#define C26 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC26, .nport = GPIO_DI_GPIOC, .npin = 26})
#define C27 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC27, .nport = GPIO_DI_GPIOC, .npin = 27})
#define C28 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC28, .nport = GPIO_DI_GPIOC, .npin = 28})
#define C29 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC29, .nport = GPIO_DI_GPIOC, .npin = 29})
#define C30 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC30, .nport = GPIO_DI_GPIOC, .npin = 30})
#define C31 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PC31, .nport = GPIO_DI_GPIOC, .npin = 31})

#define D0 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD00, .nport = GPIO_DI_GPIOD, .npin = 0})
#define D1 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD01, .nport = GPIO_DI_GPIOD, .npin = 1})
#define D2 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD02, .nport = GPIO_DI_GPIOD, .npin = 2})
#define D3 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD03, .nport = GPIO_DI_GPIOD, .npin = 3})
#define D4 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD04, .nport = GPIO_DI_GPIOD, .npin = 4})
#define D5 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD05, .nport = GPIO_DI_GPIOD, .npin = 5})
#define D6 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD06, .nport = GPIO_DI_GPIOD, .npin = 6})
#define D7 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD07, .nport = GPIO_DI_GPIOD, .npin = 7})
#define D8 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD08, .nport = GPIO_DI_GPIOD, .npin = 8})
#define D9 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD09, .nport = GPIO_DI_GPIOD, .npin = 9})
#define D10 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD10, .nport = GPIO_DI_GPIOD, .npin = 10})
#define D11 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD11, .nport = GPIO_DI_GPIOD, .npin = 11})
#define D12 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD12, .nport = GPIO_DI_GPIOD, .npin = 12})
#define D13 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD13, .nport = GPIO_DI_GPIOD, .npin = 13})
#define D14 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD14, .nport = GPIO_DI_GPIOD, .npin = 14})
#define D15 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD15, .nport = GPIO_DI_GPIOD, .npin = 15})
#define D16 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD16, .nport = GPIO_DI_GPIOD, .npin = 16})
#define D17 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD17, .nport = GPIO_DI_GPIOD, .npin = 17})
#define D18 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD18, .nport = GPIO_DI_GPIOD, .npin = 18})
#define D19 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD19, .nport = GPIO_DI_GPIOD, .npin = 19})
#define D20 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD20, .nport = GPIO_DI_GPIOD, .npin = 20})
#define D21 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD21, .nport = GPIO_DI_GPIOD, .npin = 21})
#define D22 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD22, .nport = GPIO_DI_GPIOD, .npin = 22})
#define D23 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD23, .nport = GPIO_DI_GPIOD, .npin = 23})
#define D24 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD24, .nport = GPIO_DI_GPIOD, .npin = 24})
#define D25 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD25, .nport = GPIO_DI_GPIOD, .npin = 25})
#define D26 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD26, .nport = GPIO_DI_GPIOD, .npin = 26})
#define D27 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD27, .nport = GPIO_DI_GPIOD, .npin = 27})
#define D28 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD28, .nport = GPIO_DI_GPIOD, .npin = 28})
#define D29 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD29, .nport = GPIO_DI_GPIOD, .npin = 29})
#define D30 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD30, .nport = GPIO_DI_GPIOD, .npin = 30})
#define D31 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PD31, .nport = GPIO_DI_GPIOD, .npin = 31})

#define E0 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE00, .nport = GPIO_DI_GPIOE, .npin = 0})
#define E1 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE01, .nport = GPIO_DI_GPIOE, .npin = 1})
#define E2 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE02, .nport = GPIO_DI_GPIOE, .npin = 2})
#define E3 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE03, .nport = GPIO_DI_GPIOE, .npin = 3})
#define E4 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE04, .nport = GPIO_DI_GPIOE, .npin = 4})
#define E5 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE05, .nport = GPIO_DI_GPIOE, .npin = 5})
#define E6 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE06, .nport = GPIO_DI_GPIOE, .npin = 6})
#define E7 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE07, .nport = GPIO_DI_GPIOE, .npin = 7})
#define E8 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE08, .nport = GPIO_DI_GPIOE, .npin = 8})
#define E9 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE09, .nport = GPIO_DI_GPIOE, .npin = 9})
#define E10 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE10, .nport = GPIO_DI_GPIOE, .npin = 10})
#define E11 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE11, .nport = GPIO_DI_GPIOE, .npin = 11})
#define E12 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE12, .nport = GPIO_DI_GPIOE, .npin = 12})
#define E13 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE13, .nport = GPIO_DI_GPIOE, .npin = 13})
#define E14 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE14, .nport = GPIO_DI_GPIOE, .npin = 14})
#define E15 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE15, .nport = GPIO_DI_GPIOE, .npin = 15})
#define E16 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE16, .nport = GPIO_DI_GPIOE, .npin = 16})
#define E17 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE17, .nport = GPIO_DI_GPIOE, .npin = 17})
#define E18 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE18, .nport = GPIO_DI_GPIOE, .npin = 18})
#define E19 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE19, .nport = GPIO_DI_GPIOE, .npin = 19})
#define E20 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE20, .nport = GPIO_DI_GPIOE, .npin = 20})
#define E21 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE21, .nport = GPIO_DI_GPIOE, .npin = 21})
#define E22 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE22, .nport = GPIO_DI_GPIOE, .npin = 22})
#define E23 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE23, .nport = GPIO_DI_GPIOE, .npin = 23})
#define E24 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE24, .nport = GPIO_DI_GPIOE, .npin = 24})
#define E25 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE25, .nport = GPIO_DI_GPIOE, .npin = 25})
#define E26 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE26, .nport = GPIO_DI_GPIOE, .npin = 26})
#define E27 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE27, .nport = GPIO_DI_GPIOE, .npin = 27})
#define E28 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE28, .nport = GPIO_DI_GPIOE, .npin = 28})
#define E29 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE29, .nport = GPIO_DI_GPIOE, .npin = 29})
#define E30 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE30, .nport = GPIO_DI_GPIOE, .npin = 30})
#define E31 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PE31, .nport = GPIO_DI_GPIOE, .npin = 31})

#define F0 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF00, .nport = GPIO_DI_GPIOF, .npin = 0})
#define F1 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF01, .nport = GPIO_DI_GPIOF, .npin = 1})
#define F2 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF02, .nport = GPIO_DI_GPIOF, .npin = 2})
#define F3 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF03, .nport = GPIO_DI_GPIOF, .npin = 3})
#define F4 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF04, .nport = GPIO_DI_GPIOF, .npin = 4})
#define F5 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF05, .nport = GPIO_DI_GPIOF, .npin = 5})
#define F6 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF06, .nport = GPIO_DI_GPIOF, .npin = 6})
#define F7 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF07, .nport = GPIO_DI_GPIOF, .npin = 7})
#define F8 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF08, .nport = GPIO_DI_GPIOF, .npin = 8})
#define F9 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF09, .nport = GPIO_DI_GPIOF, .npin = 9})
#define F10 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF10, .nport = GPIO_DI_GPIOF, .npin = 10})
#define F11 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF11, .nport = GPIO_DI_GPIOF, .npin = 11})
#define F12 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF12, .nport = GPIO_DI_GPIOF, .npin = 12})
#define F13 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF13, .nport = GPIO_DI_GPIOF, .npin = 13})
#define F14 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF14, .nport = GPIO_DI_GPIOF, .npin = 14})
#define F15 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF15, .nport = GPIO_DI_GPIOF, .npin = 15})
#define F16 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF16, .nport = GPIO_DI_GPIOF, .npin = 16})
#define F17 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF17, .nport = GPIO_DI_GPIOF, .npin = 17})
#define F18 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF18, .nport = GPIO_DI_GPIOF, .npin = 18})
#define F19 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF19, .nport = GPIO_DI_GPIOF, .npin = 19})
#define F20 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF20, .nport = GPIO_DI_GPIOF, .npin = 20})
#define F21 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF21, .nport = GPIO_DI_GPIOF, .npin = 21})
#define F22 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF22, .nport = GPIO_DI_GPIOF, .npin = 22})
#define F23 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF23, .nport = GPIO_DI_GPIOF, .npin = 23})
#define F24 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF24, .nport = GPIO_DI_GPIOF, .npin = 24})
#define F25 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF25, .nport = GPIO_DI_GPIOF, .npin = 25})
#define F26 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF26, .nport = GPIO_DI_GPIOF, .npin = 26})
#define F27 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF27, .nport = GPIO_DI_GPIOF, .npin = 27})
#define F28 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF28, .nport = GPIO_DI_GPIOF, .npin = 28})
#define F29 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF29, .nport = GPIO_DI_GPIOF, .npin = 29})
#define F30 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF30, .nport = GPIO_DI_GPIOF, .npin = 30})
#define F31 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PF31, .nport = GPIO_DI_GPIOF, .npin = 31})

#define X0 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX00, .nport = GPIO_DI_GPIOX, .npin = 0})
#define X1 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX01, .nport = GPIO_DI_GPIOX, .npin = 1})
#define X2 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX02, .nport = GPIO_DI_GPIOX, .npin = 2})
#define X3 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX03, .nport = GPIO_DI_GPIOX, .npin = 3})
#define X4 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX04, .nport = GPIO_DI_GPIOX, .npin = 4})
#define X5 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX05, .nport = GPIO_DI_GPIOX, .npin = 5})
#define X6 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX06, .nport = GPIO_DI_GPIOX, .npin = 6})
#define X7 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX07, .nport = GPIO_DI_GPIOX, .npin = 7})
#define X8 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX08, .nport = GPIO_DI_GPIOX, .npin = 8})
#define X9 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX09, .nport = GPIO_DI_GPIOX, .npin = 9})
#define X10 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX10, .nport = GPIO_DI_GPIOX, .npin = 10})
#define X11 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX11, .nport = GPIO_DI_GPIOX, .npin = 11})
#define X12 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX12, .nport = GPIO_DI_GPIOX, .npin = 12})
#define X13 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX13, .nport = GPIO_DI_GPIOX, .npin = 13})
#define X14 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX14, .nport = GPIO_DI_GPIOX, .npin = 14})
#define X15 ((pin_t){.ioc = HPM_IOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PX15, .nport = GPIO_DI_GPIOX, .npin = 15})

#define Y0 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY00, .nport = GPIO_DI_GPIOY, .npin = 0})
#define Y1 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY01, .nport = GPIO_DI_GPIOY, .npin = 1})
#define Y2 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY02, .nport = GPIO_DI_GPIOY, .npin = 2})
#define Y3 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY03, .nport = GPIO_DI_GPIOY, .npin = 3})
#define Y4 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY04, .nport = GPIO_DI_GPIOY, .npin = 4})
#define Y5 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY05, .nport = GPIO_DI_GPIOY, .npin = 5})
#define Y6 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY06, .nport = GPIO_DI_GPIOY, .npin = 6})
#define Y7 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY07, .nport = GPIO_DI_GPIOY, .npin = 7})
#define Y8 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY08, .nport = GPIO_DI_GPIOY, .npin = 8})
#define Y9 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY09, .nport = GPIO_DI_GPIOY, .npin = 9})
#define Y10 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY10, .nport = GPIO_DI_GPIOY, .npin = 10})
#define Y11 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY11, .nport = GPIO_DI_GPIOY, .npin = 11})
#define Y12 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY12, .nport = GPIO_DI_GPIOY, .npin = 12})
#define Y13 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY13, .nport = GPIO_DI_GPIOY, .npin = 13})
#define Y14 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY14, .nport = GPIO_DI_GPIOY, .npin = 14})
#define Y15 ((pin_t){.ioc = HPM_PIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PY15, .nport = GPIO_DI_GPIOY, .npin = 15})

#define Z0 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ00, .nport = GPIO_DI_GPIOZ, .npin = 0})
#define Z1 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ01, .nport = GPIO_DI_GPIOZ, .npin = 1})
#define Z2 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ02, .nport = GPIO_DI_GPIOZ, .npin = 2})
#define Z3 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ03, .nport = GPIO_DI_GPIOZ, .npin = 3})
#define Z4 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ04, .nport = GPIO_DI_GPIOZ, .npin = 4})
#define Z5 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ05, .nport = GPIO_DI_GPIOZ, .npin = 5})
#define Z6 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ06, .nport = GPIO_DI_GPIOZ, .npin = 6})
#define Z7 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ07, .nport = GPIO_DI_GPIOZ, .npin = 7})
#define Z8 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ08, .nport = GPIO_DI_GPIOZ, .npin = 8})
#define Z9 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ09, .nport = GPIO_DI_GPIOZ, .npin = 9})
#define Z10 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ10, .nport = GPIO_DI_GPIOZ, .npin = 10})
#define Z11 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ11, .nport = GPIO_DI_GPIOZ, .npin = 11})
#define Z12 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ12, .nport = GPIO_DI_GPIOZ, .npin = 12})
#define Z13 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ13, .nport = GPIO_DI_GPIOZ, .npin = 13})
#define Z14 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ14, .nport = GPIO_DI_GPIOZ, .npin = 14})
#define Z15 ((pin_t){.ioc = HPM_BIOC, .ngpio = HPM_GPIO0, .npad = IOC_PAD_PZ15, .nport = GPIO_DI_GPIOZ, .npin = 15})
