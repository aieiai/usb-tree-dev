#ifndef HAL_LIGHTING_H
#define HAL_LIGHTING_H

#include <stdint.h>

#include "hal_usb.h"

/* 
 * 灯效接口：interface 2。HID 报告都是 Feature 报告（走 EP0）
 *
 * - HAL_LIGHTING_LAMPARRAY_ENABLE = 0：不接管 HID Feature 报告读写，
 *   行为与未实现时一致（Windows 能识别设备，但拿不到灯阵属性）。
 * - = 1：按 HID LampArray 简版应答。注意：当前这组应答在 Windows 上
 *   仍会让设备从「动态光效」列表里消失（属性内容需按真实灯珠校准），
 *   所以默认关闭。
 */
#ifndef HAL_LIGHTING_LAMPARRAY_ENABLE
#define HAL_LIGHTING_LAMPARRAY_ENABLE (0)
#endif

/* 灯珠数量：按你们键盘的实际灯珠数改 */
#ifndef HAL_LIGHTING_LAMP_COUNT
#define HAL_LIGHTING_LAMP_COUNT      (40U)
#endif

#define HAL_LIGHTING_LAMP_PITCH_UM   (19000U)  /* 相邻灯珠间距（键距 19mm） */
#define HAL_LIGHTING_LAMP_SIZE_UM    (15000U)  /* 单颗灯珠尺寸，用于包围盒高度 */
#define HAL_LIGHTING_LAMP_DEPTH_UM   (3000U)   /* 包围盒厚度 */

/* 业务层实现：把颜色写到对应灯珠。
 * lamp_id 范围 0 .. HAL_LIGHTING_LAMP_COUNT-1，各通道 0..255。
 * 默认是空实现，应用里定义同名函数（非 weak）即可覆盖。 */
void hal_lighting_set_color(uint16_t lamp_id, uint8_t red, uint8_t green, uint8_t blue, uint8_t intensity);


#endif
