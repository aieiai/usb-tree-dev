#ifndef HID_DESCRIPTOR_H
#define HID_DESCRIPTOR_H
#include <stdint.h>
#include <string.h>

#include "usb_config.h"

#ifndef HAL_USB_CC_WINUSB_ENABLE
#define HAL_USB_CC_WINUSB_ENABLE        (1)
#endif

#ifndef HAL_USB_DT_WINUSB_ENABLE
#define HAL_USB_DT_WINUSB_ENABLE        (1)
#endif

/* 是否声明 WebUSB 平台能力（浏览器访问）。
 * Windows 上 WebUSB 靠 WinUSB 才能打开接口，所以依赖 DT 的 WinUSB 开关；
 * Linux/macOS 不需要 WinUSB（接口无内核驱动即可），但也只有 DT 接口存在才有意义。 */
#ifndef HAL_USB_WEBUSB_ENABLE
#define HAL_USB_WEBUSB_ENABLE           (1)
#endif


#define HAL_USB_WINUSB_ENABLE           (HAL_USB_CC_WINUSB_ENABLE || HAL_USB_DT_WINUSB_ENABLE)

#define USB_PRODUCT_VERSION             (0x0100)

typedef struct {
    const uint8_t *buf;
    const uint32_t len;
} usb_descriptor_t;

/* 手柄模式：影响配置描述符与设备描述符的选择 */
typedef enum {
    HAL_GAMEPAD_MODE_OFF = 0,
    HAL_GAMEPAD_MODE_HID,
    HAL_GAMEPAD_MODE_XBOX,
} gamepad_mode_ty;

extern volatile gamepad_mode_ty g_gamepad_mode;

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_INTF_KBD,
    STRID_INTF_CUSTOM,
    STRID_INTF_LIGHTING,
    STRID_INTF_DT,
    STRID_INTF_GAMEPAD_HID,
    STRID_INTF_GAMEPAD_XBOX,
    STRID_COUNT
};

enum {
    REPORT_ID_NKRO     = 0x01,
    REPORT_ID_CONSUMER = 0x02,
    REPORT_ID_SYSTEM   = 0x03,
    REPORT_ID_6KRO     = 0x04,
    REPORT_ID_MOUSE    = 0x05,
    REPORT_ID_MAX
};

enum {
    LIGHTING_REPORT_ID_ATTRIBUTES    = 0x01,
    LIGHTING_REPORT_ID_ATTR_REQUEST  = 0x02,
    LIGHTING_REPORT_ID_ATTR_RESPONSE = 0x03,
    LIGHTING_REPORT_ID_MULTI_UPDATE  = 0x04,
    LIGHTING_REPORT_ID_RANGE_UPDATE  = 0x05,
    LIGHTING_REPORT_ID_CONTROL       = 0x06,
};



#define USBD_INTERVAL_MS(ms)            ((uint16_t)((ms) * (1000 / 125)))

#define INTF_KBD_NUM                    (0x00) /* Interface number for keyboard */
#define INTF_KBD_EPI                    (0x81) /* Endpoint number and attributes for keyboard input */
#define INTF_KBD_EPI_SIZE               (0x40)
#define INTF_KBD_EPI_INTERVAL           (USBD_INTERVAL_MS(0.125))
#define INTF_KBD_EPO                    (0x01) /* Endpoint number and attributes for keyboard output */
#define INTF_KBD_EPO_SIZE               (0x08)
#define INTF_KBD_EPO_INTERVAL           (USBD_INTERVAL_MS(0.125))


#define INTF_CC_NUM                     (0x01) /* Interface number for Upper computer control center */
#define INTF_CC_EPI                     (0x82) /* Endpoint number and attributes for Upper computer input control center */
#define INTF_CC_EP_SIZE                 (64)
#define INTF_CC_EP_INTERVAL             (USBD_INTERVAL_MS(0.125))
#define INTF_CC_EPO                     (0x02) /* Endpoint number and attributes for Upper computer output control center */


#define INTF_LIGHTING_NUM               (0x02) /* Interface number for Lighting */
#define INTF_LIGHTING_EPI               (0x83) /* Endpoint number and attributes for Lighting input */
#define INTF_LIGHTING_EPI_SIZE          (8)
#define INTF_LIGHTING_EPI_INTERVAL      (USBD_INTERVAL_MS(16))
#define INTF_LIGHTING_EPO               (0x03) /* Endpoint number and attributes for Lighting output */
#define INTF_LIGHTING_EPO_SIZE          (8)
#define INTF_LIGHTING_EPO_INTERVAL      (USBD_INTERVAL_MS(16)) /* max: 60fps */


#define INTF_DT_NUM                     (0x03) /* Interface number for Diagnostic Tracking */
#define INTF_DT_EPI                     (0x84) /* Endpoint number and attributes for Diagnostic Tracking input */
#define INTF_DT_EPO                     (0x04) /* Endpoint number and attributes for Diagnostic Tracking output */
#if HAL_USB_DT_WINUSB_ENABLE
    #define INTF_DT_EP_TYPE             (0x02)
    #define INTF_DT_EPI_INTERVAL        (USBD_INTERVAL_MS(0))    /* 批量端点不使用轮询间隔 */
    #define INTF_DT_EPO_INTERVAL        (USBD_INTERVAL_MS(0))
    #if defined(CONFIG_USB_HS)
        #define INTF_DT_EP_SIZE         USB_BULK_EP_MPS_HS /* The high speed can only be 512 */
    #else
        #define INTF_DT_EP_SIZE         USB_BULK_EP_MPS_FS  /* The full speed can only be 8/16/32/64 */
    #endif
#else
    #define INTF_DT_EP_TYPE             (0x03)
    #define INTF_DT_EPI_INTERVAL        (USBD_INTERVAL_MS(1))
    #define INTF_DT_EPO_INTERVAL        (USBD_INTERVAL_MS(1))
    #define INTF_DT_EP_SIZE             (64)
#endif


/* gamepad 排在 Diagnostic Tracking 之后：未启用 gamepad 时接口号依然连续（1..4）
 *
 * 两种模式由 g_gamepad_mode 选择，走不同的配置描述符：
 *  - HAL_GAMEPAD_MODE_HID  : 标准 HID 接口（class 0x03），免驱，13 字节报告，见 REPORT_DESC_GAMEPAD；
 *  - HAL_GAMEPAD_MODE_XBOX : vendor class 0xFF/0x5D/0x01 + 0x21 私有描述符 + 20 字节报告。
 *    Xbox 模式下 MS OS 2.0 描述符集会额外给本接口挂 CompatibleID="XUSB10"，
 *    Windows 据此加载 xusb22.sys（XInput），因此 VID/PID 可以用任意自定义值。
 *    该模式通过独立的 MS OS 2.0 XUSB10 子集绑定 xusb22.sys，不依赖 CC/DT 的 WinUSB 开关；
 *    Xbox 模式会自动上报 USB 2.1 设备描述符与 BOS。
 *    Linux 的 xpad 驱动按接口 class/subclass/protocol 匹配，同样可直接识别。 */
#define INTF_GAMEPAD_NUM                (0x04) /* Interface number for Gamepad for standard hid or xbox controller */
// standard hid gamepad
#define INTF_GAMEPAD_HID_EPI            (0x85) /* Endpoint number and attributes for Gamepad for standard hid input */
#define INTF_GAMEPAD_HID_EPI_SIZE       (32)   /* 端点包大小，需 >= 13 字节报告 */
#define INTF_GAMEPAD_HID_EPI_INTERVAL   (USBD_INTERVAL_MS(0.5)) /* 4ms */
#define INTF_GAMEPAD_HID_EPO            (0x05) /* Endpoint number and attributes for Gamepad for standard hid output */
#define INTF_GAMEPAD_HID_EPO_SIZE       (8)
#define INTF_GAMEPAD_HID_EPO_INTERVAL   (USBD_INTERVAL_MS(0.5))
/* HID 模式 IN 报告长度，与 DESC_RPT_GAMEPAD 的 13 字节严格对应 */
#define INTF_GAMEPAD_HID_RPT_LEN        (13)
// Xbox 360 Controller for Windows
#define INTF_GAMEPAD_XBOX_EPI           (0x85) /* Endpoint number and attributes for Gamepad xbox input */
#define INTF_GAMEPAD_XBOX_EPI_SIZE      (32)   /* 端点包大小，需 >= 20 字节报告 */
#define INTF_GAMEPAD_XBOX_EPI_INTERVAL  (USBD_INTERVAL_MS(0.5)) /* 4ms */
#define INTF_GAMEPAD_XBOX_EPO           (0x05) /* Endpoint number and attributes for Gamepad xbox output */
#define INTF_GAMEPAD_XBOX_EPO_SIZE      (32)   /* 端点包大小，需 >= 8 字节震动报告 */
#define INTF_GAMEPAD_XBOX_EPO_INTERVAL  (USBD_INTERVAL_MS(1))   /* 8ms */
/* Xbox 360 输入包固定 20 字节（前 2 字节为 0x00 / 0x14） */
#define INTF_GAMEPAD_XBOX_RPT_LEN       (20)
/* Xbox 360 震动包固定 8 字节（前 2 字节为 0x00 / 0x08） */
#define INTF_GAMEPAD_XBOX_OUT_RPT_LEN   (8)


/* ---- 两个 WinUSB 功能独立开关的定义在最前面（HAL_USB_CC_WINUSB_ENABLE / HAL_USB_DT_WINUSB_ENABLE）---- */

/* 同时声明两个平台能力：
 *  - Microsoft OS 2.0：让 Windows 自动把 DT 接口绑定到 WinUSB；
 *  - WebUSB：让浏览器（Chrome/Edge）能用 WebUSB API 访问同一个接口。
 * 两者的 vendor code 必须不同，否则厂商请求会互相抢答（cherryusb 按 bRequest 分发）。 */
#define MSOS_VENDOR_CODE                (0x07)
#define WEBUSB_VENDOR_CODE              (0x08)
#define WEBUSB_LANDING_PAGE_CODE        (0x01)

#define HAL_USB_CC_GUID                 "{1F3C7A21-8B4E-4D95-A6C1-2E7B9F4D5A38}"
#define HAL_USB_DT_GUID                 "{6B1D7C3E-4A2F-4E8B-9C5D-1F0A2B3C4D5E}"


extern const struct usb_descriptor g_hid_descriptor;
extern const usb_descriptor_t g_desc_rpt_kbd;
extern const usb_descriptor_t g_desc_rpt_lighting;
extern const usb_descriptor_t g_desc_rpt_gamepad;

/* 按 g_gamepad_mode 选好 BOS / MS OS 2.0 描述符集（Xbox 模式含 XUSB10 子集），
 * 在 usbd_desc_register() 之前调用。 */
void hid_descriptor_init(void);


#endif
