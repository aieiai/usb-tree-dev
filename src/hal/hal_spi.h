#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>
#include "hpm_common.h"

/* 1 = 中断方式：发送完成后在 SPI 中断里回调
 * 0 = 无中断：仅按发送起始时刻的最小间隔节流（默认） */
#ifndef HAL_SPI_TX_IRQ_ENABLE
#define HAL_SPI_TX_IRQ_ENABLE (0)
#endif

/* SPI 时钟固定 6MHz，一个 SPI bit 约 0.167us */
#define HAL_SPI_BITRATE_HZ (6000000UL)

#if HAL_SPI_TX_IRQ_ENABLE
/* 发送完成回调，在 SPI 中断里执行，不能阻塞 */
typedef void (*hal_spi_tx_done_cb_t)(void);
void hal_spi_set_tx_done_callback(hal_spi_tx_done_cb_t cb);
#endif

hpm_stat_t hal_spi_init(void);

/* 非阻塞发送：启动 DMA 后立即返回。
 * data 需保持有效直到本次 DMA 发送完成；
 * 距上一次发送起始不足最小间隔时返回 status_spi_master_busy。 */
hpm_stat_t hal_spi_write(const uint8_t *data, uint32_t size);

#endif
