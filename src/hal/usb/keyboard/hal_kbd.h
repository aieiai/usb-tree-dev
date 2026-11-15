#ifndef HAL_KBD_H
#define HAL_KBD_H

#include <stdint.h>
#include <string.h>


/* 键盘接口（interface 0）：IN 报告发送 + 主机下发的 LED Output 报告 */

/* 单个 IN 报告的最大长度：NKRO 报告为 1 + 29 字节，这里留出余量 */
#define HAL_KBD_REPORT_MAX  (32U)
/* LED Output 报告长度（EP 0x01 的 mps 是 8） */
#define HAL_KBD_LED_OUT_MAX (8U)

/* 通过键盘 IN 端点发送一个中断报告，阻塞等待传输完成。
 * 返回 0 表示成功，负值表示未配置 / 端点忙 / 等待超时。 */
int hal_kbd_send(const uint8_t *report, uint16_t len, uint32_t timeout_ms);

/* 主机下发的 LED 位图：bit0..4 = Num/Caps/Scroll/Compose/Kana */
uint8_t hal_kbd_get_leds(void);


#endif
