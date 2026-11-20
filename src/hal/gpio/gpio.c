/*
 * Copyright 2025 HPMicro
 * SPDX-License-Identifier: Apache-2.0
 */
#include "hpm_gpio_drv.h"

uint8_t hpmicro_gpio_read_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin) {
    return gpio_read_pin(ptr, port, pin);
}

void hpmicro_gpio_toggle_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin) {
    return gpio_toggle_pin(ptr, port, pin);
}

void hpmicro_gpio_write_pin(GPIO_Type *ptr, uint32_t port, uint8_t pin, uint8_t high) {
    return gpio_write_pin(ptr, port, pin, high);
}

void hpmicro_gpio_set_pin_input(GPIO_Type *ptr, uint32_t port, uint8_t pin) {
    return gpio_set_pin_input(ptr, port, pin);
}

void hpmicro_gpio_set_pin_output(GPIO_Type *ptr, uint32_t port, uint8_t pin) {
    return gpio_set_pin_output(ptr, port, pin);
}
