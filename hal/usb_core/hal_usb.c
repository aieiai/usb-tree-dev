#include <string.h>

#include "hal_usb.h"

#include "hal_kbd.h"
#include "hal_cc.h"
#include "hal_lighting.h"
#include "hal_dt.h"

#include "hid_descriptor.h"

#include "board.h"
#include "usb_osal.h"
#include "usbd_core.h"

volatile gamepad_mode_ty g_gamepad_mode = HAL_GAMEPAD_MODE_OFF;

/* 各功能模块导出的注册项。这里的顺序 = 配置描述符里的接口顺序。 */
extern const struct hal_usb_function hal_kbd_function;
extern const struct hal_usb_function hal_cc_function;
extern const struct hal_usb_function hal_lighting_function;
extern const struct hal_usb_function hal_dt_function;

static const struct hal_usb_function *const s_usb_functions[] = {
    &hal_kbd_function,
    &hal_cc_function,
    &hal_lighting_function,
    &hal_dt_function,
};

#define HAL_USB_FUNCTION_COUNT (sizeof(s_usb_functions) / sizeof(s_usb_functions[0]))


static void hal_usb_event_dispatch(uint8_t busid, uint8_t event)
{
    uint32_t i;

    switch (event) {
    case USBD_EVENT_RESET:
        USB_LOG_RAW("[usb] reset, port speed=%u (2=full 3=high)\r\n", usbd_get_port_speed(busid));
        break;
    case USBD_EVENT_CONNECTED:
        USB_LOG_RAW("[usb] connected\r\n");
        break;
    case USBD_EVENT_DISCONNECTED:
        USB_LOG_RAW("[usb] disconnected\r\n");
        break;
    case USBD_EVENT_CONFIGURED:
        USB_LOG_RAW("[usb] configured\r\n");
        break;
    default:
        break;
    }

    for (i = 0; i < HAL_USB_FUNCTION_COUNT; i++) {
        if (s_usb_functions[i]->event != NULL) {
            s_usb_functions[i]->event(busid, event);
        }
        if ((event == USBD_EVENT_CONFIGURED) && (s_usb_functions[i]->configured != NULL)) {
            s_usb_functions[i]->configured(busid);
        }
    }
}

void hal_usb_init(void)
{
    uint32_t i;

    usbd_desc_register(CONFIG_USBD_BUSID, &g_hid_descriptor);

    for (i = 0; i < HAL_USB_FUNCTION_COUNT; i++) {
        if ((s_usb_functions[i]->init != NULL) && (s_usb_functions[i]->intf_num == i)) {
            s_usb_functions[i]->init(CONFIG_USBD_BUSID);
        }
    }

    usbd_initialize(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE, hal_usb_event_dispatch);

    USB_LOG_RAW("[usb] init done\r\n");
}
