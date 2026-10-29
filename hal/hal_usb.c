#include "hal_usb.h"

#include "usb_osal.h"
#include "usbd_core.h"


#include "board.h"

volatile gamepad_mode_ty g_gamepad_mode = HAL_GAMEPAD_MODE_OFF;

/* 各功能模块导出的注册项。这里的顺序 = 配置描述符里的接口顺序。 */
extern const hal_usb_fn_t hal_kbd_function;
extern const hal_usb_fn_t hal_cc_function;
extern const hal_usb_fn_t hal_lighting_function;
extern const hal_usb_fn_t hal_dt_function;
extern const hal_usb_fn_t hal_gamepad_function;

static const hal_usb_fn_t *const s_usb_functions[] = {
    &hal_kbd_function,
    &hal_cc_function,
    &hal_lighting_function,
    &hal_dt_function,
    &hal_gamepad_function,
};

#define HAL_USB_FUNCTION_COUNT (sizeof(s_usb_functions) / sizeof(s_usb_functions[0]))


/* 手柄模式为 OFF 时函数表里仍保留 gamepad，但不注册接口、不派发事件 */
static int hal_usb_function_present(const hal_usb_fn_t *fn)
{
    return (fn->present == NULL) || (fn->present() != 0U);
}

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
        if (!hal_usb_function_present(s_usb_functions[i])) {
            continue;
        }
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

    hid_descriptor_init();

    usbd_desc_register(CONFIG_USBD_BUSID, &g_hid_descriptor);

    for (i = 0; i < HAL_USB_FUNCTION_COUNT; i++) {
        if (!hal_usb_function_present(s_usb_functions[i])) {
            continue;
        }
        if ((s_usb_functions[i]->init != NULL) && (s_usb_functions[i]->intf_num == i)) {
            s_usb_functions[i]->init(CONFIG_USBD_BUSID);
        }
    }

    usbd_initialize(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE, hal_usb_event_dispatch);

    USB_LOG_RAW("[usb] init done, gamepad mode=%u\r\n", (unsigned)g_gamepad_mode);
}

void hal_usb_reconfigure(void)
{
    // 模式切换改变了接口集合，只能让主机重新枚举：断开 -> 重新注册描述符与各功能 -> 重新连接
    (void)usbd_deinitialize(CONFIG_USBD_BUSID);
    hal_usb_init();
}

int hal_usb_gamepad_set_mode(gamepad_mode_ty mode) {
    if ((mode != HAL_GAMEPAD_MODE_OFF) && (mode != HAL_GAMEPAD_MODE_HID) && (mode != HAL_GAMEPAD_MODE_XBOX)) {
        return -1;
    }
    if (mode == g_gamepad_mode) {
        return 0;
    }

    g_gamepad_mode = mode;

    hal_usb_reconfigure();
    return 0;
}

gamepad_mode_ty hal_usb_gamepad_get_mode(void) {
    return g_gamepad_mode;
}


