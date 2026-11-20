#ifndef DRV_WS2812_H
#define DRV_WS2812_H

#include <stdint.h>
#include "hpm_common.h"

/* 灯珠数量，可在编译选项里覆盖 */
#ifndef WS2812_LED_MAX
#define WS2812_LED_MAX (42 * 2)
#endif

hpm_stat_t drv_ws2812_init(void);
void drv_ws2812_set_pixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void drv_ws2812_set_all(uint8_t r, uint8_t g, uint8_t b);
void drv_ws2812_clear(void);
hpm_stat_t drv_ws2812_show(void);

#endif
