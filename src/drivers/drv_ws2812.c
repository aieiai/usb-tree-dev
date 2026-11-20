#include <string.h>

#include "drv_ws2812.h"
#include "hal_spi.h"

/*
 * SPI 6MHz 时一个 SPI 字节对应一个 WS2812 bit（约 1.333us）：
 *   0 -> 0xC0（11000000）高约 0.33us，低约 1.0us
 *   1 -> 0xF0（11110000）高约 0.67us，低约 0.67us
 * A pixel is 24bit (GRB), Frame to frame MOSI is low.
 */
#define WS2812_SPI_BYTE_ZERO     (0xC0U)
#define WS2812_SPI_BYTE_ONE      (0xF0U)
#define WS2812_BITS_PER_COLOR    (8U)
#define WS2812_SPI_BYTES_PER_LED (WS2812_BITS_PER_COLOR * 3U)

#define WS2812_DATA_BYTES        (WS2812_LED_MAX * WS2812_SPI_BYTES_PER_LED)

// ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(4)
ATTR_PLACE_AT_NONCACHEABLE_BSS_WITH_ALIGNMENT(4)
static uint8_t s_tx_buf[WS2812_DATA_BYTES];

hpm_stat_t drv_ws2812_init(void)
{
    drv_ws2812_clear();
    return hal_spi_init();
}

void drv_ws2812_set_pixel(uint16_t li, uint8_t r, uint8_t g, uint8_t b)
{
    if (li >= WS2812_LED_MAX) {
        return;
    }

    // WS2812 data order is G R B
    uint8_t *buf = &s_tx_buf[li * WS2812_SPI_BYTES_PER_LED];
    for (uint32_t bit = 0; bit < WS2812_BITS_PER_COLOR; bit++) {
        buf[bit+0]  = ((g & (uint8_t)(0x80U >> bit)) != 0U) ? WS2812_SPI_BYTE_ONE : WS2812_SPI_BYTE_ZERO;
        buf[bit+8]  = ((r & (uint8_t)(0x80U >> bit)) != 0U) ? WS2812_SPI_BYTE_ONE : WS2812_SPI_BYTE_ZERO;
        buf[bit+16] = ((b & (uint8_t)(0x80U >> bit)) != 0U) ? WS2812_SPI_BYTE_ONE : WS2812_SPI_BYTE_ZERO;
    }
}

void drv_ws2812_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t i;

    for (i = 0; i < WS2812_LED_MAX; i++) {
        drv_ws2812_set_pixel(i, r, g, b);
    }
}

void drv_ws2812_clear(void)
{
    memset(s_tx_buf, WS2812_SPI_BYTE_ZERO, WS2812_DATA_BYTES);
}

hpm_stat_t drv_ws2812_show(void)
{
    return hal_spi_write(s_tx_buf, sizeof(s_tx_buf));
}
