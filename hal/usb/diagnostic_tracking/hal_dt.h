#ifndef HAL_DT_H
#define HAL_DT_H

#include <stdint.h>
#include "../descriptor/hid_descriptor.h"


/* 诊断追踪通道（Diagnostic Tracking 接口，interface 3）：
 *  - WinUSB 模式：厂商自定义接口 + 批量端点；
 *  - HID 退回模式（WinUSB 关）：HID 接口 + 中断端点，免驱。
 * 两种模式都有：
 *  - IN  端点：设备 -> 上位机，数据先进环形缓冲，再由独立线程搬运发送，调用方永不阻塞；
 *  - OUT 端点：上位机 -> 设备，用于下发命令（例如动态调整诊断等级）。
 * WinUSB 模式下上位机按 HAL_USB_DT_GUID 枚举设备（见 hid_descriptor.h）。 */

#if HAL_USB_DT_WINUSB_ENABLE
/* 批量端点：每次搬运必须 < bulk mps(512)，保证是短包、主机能立即判定传输结束；
 * 若取 512 的整数倍就必须额外补 ZLP */
#define HAL_DT_PACKET_SIZE      (256U)
#define HAL_DT_RX_PACKET_SIZE   (512U)
#else
/* 中断端点（mps 64）：中断传输以包为单位，一包就是一帧，无需 ZLP */
#define HAL_DT_PACKET_SIZE      (64U)
#define HAL_DT_RX_PACKET_SIZE   (64U)
#endif

#define HAL_DT_RING_SIZE        (4096U)  /* IN 方向环形缓冲，必须是 2 的幂，满了丢新数据 */
#define HAL_DT_RX_RING_SIZE     (1024U)  /* OUT 方向环形缓冲，必须是 2 的幂（>= RX_PACKET_SIZE * 2） */
#define HAL_DT_TX_TIMEOUT_MS    (100U)   /* 单包发送超时：主机长时间不取数据时丢弃该包，避免卡死 */


/* 注册 DT 接口与端点，必须在 usbd_initialize() 之前调用一次 */
void hal_dt_usb_init(uint8_t busid);

/* 主机完成 SET_CONFIGURATION 后调用：唤醒发送线程并装载 OUT 端点 */
void hal_dt_usb_configured(void);

/* 写入诊断数据，可在任务或中断中调用，只做内存拷贝，不等待 USB 传输 */
void hal_dt_write(const void *data, uint32_t len);

/* 便捷接口，内部同样走环形缓冲，不会阻塞 */
void hal_dt_puts(const char *str);
void hal_dt_printf(const char *fmt, ...);

/* 因缓冲区满或发送超时被丢弃的字节数 */
uint32_t hal_dt_dropped(void);

/* 读取上位机下发的数据，返回实际长度；超时返回 -1 */
int hal_dt_read(void *buf, uint32_t len, uint32_t timeout_ms);

#endif
