#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdio.h>
#include <stdarg.h>
#include "hpm_common.h"

hpm_stat_t uart_tx_trigger_dma(uint8_t *buff, uint32_t size);
hpm_stat_t uart_rx_trigger_dma(uint8_t *buff, uint32_t size)
hpm_stat_t hal_uart_init(void);

#endif

