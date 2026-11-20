/*
 * Copyright 2025 HPMicro
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

//#include "pin_defs.h"
#include "hpm_soc.h"

#ifndef IOC_PAD_PAD_CTL_HYS_SET
#    define IOC_PAD_PAD_CTL_HYS_SET IOC_PAD_PAD_CTL_SMT_SET
#endif

#ifndef IOC_FUNC_CTL_GPIO
#    define IOC_FUNC_CTL_GPIO IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0)
#endif

/* PIOC or BIOC FUNC for SOC is 3  */
#ifndef IOC_FUNC_CTL_SOC_GPIO
#    define IOC_FUNC_CTL_SOC_GPIO IOC_PAD_FUNC_CTL_ALT_SELECT_SET(3)
#endif

#define PIOC_FUNC(pin, func, line) PIOC_P##pin##_FUNC_CTL_##func##_##line
#define IOC_FUNC(pin, func, line) IOC_P##pin##_FUNC_CTL_##func##_##line
#define IOC_ANALOG(pin, func, line) IOC_ANALOG_FUNC_CTL_##func##_##line

#define HPMICRO_GPIO_DRV_INLINE 1
#if defined(HPMICRO_GPIO_DRV_INLINE) && HPMICRO_GPIO_DRV_INLINE
/**
 * @brief   Read target pin level
 *
 * @param ptr GPIO base address
 * @param port Port index
 * @param pin Pin index
 *
 * @return Pin status mask
 */
static inline uint8_t hpmicro_gpio_read_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin) {
    return (ptr->DI[port].VALUE & (1 << pin)) >> pin;
}

/**
 * @brief   Toggle pin level
 *
 * @param ptr GPIO base address
 * @param port Port index
 * @param pin Pin index
 */
static inline void hpmicro_gpio_toggle_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin) {
    ptr->DO[port].TOGGLE = 1 << pin;
}

/**
 * @brief   Write pin level
 *
 * @param ptr GPIO base address
 * @param port Port index
 * @param pin Pin index
 * @param high Pin level set to high when it is set to true
 */
static inline void hpmicro_gpio_write_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin, uint8_t high) {
    if (high) {
        ptr->DO[port].SET = 1 << pin;
    } else {
        ptr->DO[port].CLEAR = 1 << pin;
    }
}

/**
 * @brief   Set pin to input mode
 *
 * @param ptr GPIO base address
 * @param port Port index
 * @param pin Pin index
 */
static inline void hpmicro_gpio_set_pin_input(GPIO_Type *ptr, uint32_t port, uint8_t pin) {
    ptr->OE[port].CLEAR = 1 << pin;
}

/**
 * @brief   Set pin to output mode
 *
 * @param ptr GPIO base address
 * @param port Port index
 * @param pin Pin index
 */
static inline void hpmicro_gpio_set_pin_output(GPIO_Type *ptr, uint32_t port, uint8_t pin) {
    ptr->OE[port].SET = 1 << pin;
}

#else
extern uint8_t hpmicro_gpio_read_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin);
extern void    hpmicro_gpio_toggle_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin);
extern void    hpmicro_gpio_write_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin, uint8_t high);
extern void    hpmicro_gpio_set_pin_input(GPIO_Type *ptr, uint32_t port, uint8_t pin);
extern void    hpmicro_gpio_set_pin_output(GPIO_Type *ptr, uint32_t port, uint8_t pin);
#endif

#define gpio_set_pin_alternate(xpin, func_ctl, pad_ctl)                    \
    do {                                                                   \
        HPM_IOC->PAD[(xpin).npad].FUNC_CTL = func_ctl;                     \
        HPM_IOC->PAD[(xpin).npad].PAD_CTL  = pad_ctl;                      \
        if ((xpin).ioc != HPM_IOC) {                                       \
            /* set control by IOC */                                       \
            (xpin).ioc->PAD[(xpin).npad].FUNC_CTL = IOC_FUNC_CTL_SOC_GPIO; \
        }                                                                  \
    } while (0)

#define gpio_set_pin_input(xpin)                                                     \
    do {                                                                             \
        /* set to gpio func and smt enale */                                         \
        gpio_set_pin_alternate(xpin, IOC_FUNC_CTL_GPIO, IOC_PAD_PAD_CTL_HYS_SET(1)); \
        hpmicro_gpio_set_pin_input((xpin).ngpio, (xpin).nport, (xpin).npin);         \
    } while (0)

#define gpio_set_pin_input_low(xpin)                                                                                                         \
    do {                                                                                                                                     \
        /* set to gpio func and smt enale, pull down */                                                                                      \
        gpio_set_pin_alternate(xpin, IOC_FUNC_CTL_GPIO, IOC_PAD_PAD_CTL_HYS_SET(1) | IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(0)); \
        hpmicro_gpio_set_pin_input((xpin).ngpio, (xpin).nport, (xpin).npin);                                                                 \
    } while (0)

#define gpio_set_pin_input_high(xpin)                                                                                                        \
    do {                                                                                                                                     \
        /* set to gpio func and smt enale, pull up */                                                                                        \
        gpio_set_pin_alternate(xpin, IOC_FUNC_CTL_GPIO, IOC_PAD_PAD_CTL_HYS_SET(1) | IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1)); \
        hpmicro_gpio_set_pin_input((xpin).ngpio, (xpin).nport, (xpin).npin);                                                                 \
    } while (0)

#define gpio_set_pin_output_push_pull(xpin)                                   \
    do {                                                                      \
        gpio_set_pin_alternate(xpin, IOC_FUNC_CTL_GPIO, 0);                   \
        hpmicro_gpio_set_pin_output((xpin).ngpio, (xpin).nport, (xpin).npin); \
    } while (0)

#define gpio_set_pin_output_open_drain(xpin)                                        \
    do {                                                                            \
        gpio_set_pin_alternate(xpin, IOC_FUNC_CTL_GPIO, IOC_PAD_PAD_CTL_OD_SET(1)); \
        hpmicro_gpio_set_pin_output((xpin).ngpio, (xpin).nport, (xpin).npin);       \
    } while (0)

#define gpio_set_pin_output(xpin) gpio_set_pin_output_push_pull(xpin)
#define gpio_write_pin_high(xpin) hpmicro_gpio_write_pin((xpin).ngpio, (xpin).nport, (xpin).npin, 1)
#define gpio_write_pin_low(xpin) hpmicro_gpio_write_pin((xpin).ngpio, (xpin).nport, (xpin).npin, 0)
#define gpio_write_pin(xpin, level) hpmicro_gpio_write_pin((xpin).ngpio, (xpin).nport, (xpin).npin, (level))
#define gpio_read_pin(xpin) hpmicro_gpio_read_pin((xpin).ngpio, (xpin).nport, (xpin).npin)
#define gpio_toggle_pin(xpin) hpmicro_gpio_toggle_pin((xpin).ngpio, (xpin).nport, (xpin).npin)
