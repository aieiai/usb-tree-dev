#ifndef HAL_USB_H
#define HAL_USB_H

#include <stdint.h>
#include <string.h>


#include "usb_config.h"
#include "./usb/control_center/hal_cc.h"
#include "./usb/descriptor/hid_descriptor.h"
#include "./usb/diagnostic_tracking/hal_dt.h"
#include "./usb/gamepad/hal_gamepad.h"
#include "./usb/keyboard/hal_kbd.h"
#include "./usb/lighting/hal_lighting.h"


typedef struct {
    const uint8_t intf_num;
    const char *name;
    void (*init)(uint8_t busid);
    void (*configured)(uint8_t busid);
    void (*event)(uint8_t busid, uint8_t event);
    /* 可选：返回 0 时本次枚举不注册该接口（例如手柄模式为 OFF），留 NULL 表示始终存在 */
    uint8_t (*present)(void);
} hal_usb_fn_t;

void hal_usb_init(void);
void hal_usb_reconfigure(void);
int  hal_usb_gamepad_set_mode(gamepad_mode_ty mode);
gamepad_mode_ty hal_usb_gamepad_get_mode(void);

#endif
