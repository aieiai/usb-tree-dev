#ifndef HAL_CC_H
#define HAL_CC_H

#include <stdint.h>
#include <string.h>


/* 上位机接口（interface 1）：厂商自定义 HID，双向中断端点。
 * 主机下发的数据进环形缓冲，应用侧用 hal_cc_read() 取走。 */

/* OUT 方向环形缓冲，必须是 2 的幂 */
#define HAL_CC_RX_RING_SIZE (256U)

/* 非阻塞读取主机下发的数据，返回实际长度 */
int hal_cc_read(uint8_t *buf, uint32_t len);
/* 因环形缓冲满而丢弃的字节数 */
uint32_t hal_cc_dropped(void);


#endif
