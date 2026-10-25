#ifndef HAL_USB_H
#define HAL_USB_H

#include <stdint.h>

#include "usb_config.h"
#include "usbd_core.h"

/* 每个 USB 功能模块（键盘 / 上位机 / 灯效 / 诊断追踪……）实现这组回调，
 * 由 hal_usb 在初始化与枚举事件里统一调度，模块之间互不依赖。 */
struct hal_usb_function {
    const uint8_t intf_num;
    const char *name;
    /* 注册本功能的接口与端点，在 usbd_initialize() 之前调用 */
    void (*init)(uint8_t busid);
    /* 主机完成 SET_CONFIGURATION 后调用：装载 OUT 端点、唤醒发送线程等 */
    void (*configured)(uint8_t busid);
    /* 其他 USBD 事件（RESET / CONNECTED / DISCONNECTED ...），不需要可留 NULL */
    void (*event)(uint8_t busid, uint8_t event);
};

/* 初始化 USB 复合设备：注册描述符 -> 依次初始化各功能 -> 启动控制器。
 * 各功能在 hal_usb.c 的函数表里的顺序，必须与配置描述符中的接口顺序一致。 */
void hal_usb_init(void);

#endif
