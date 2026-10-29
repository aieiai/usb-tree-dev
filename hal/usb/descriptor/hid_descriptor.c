#include "hid_descriptor.h"
#include "kbd_descriptor.h"

#include "usbd_core.h"
#include "usbd_hid.h"

#include "tu_hid_device.h"


#define USE_MSOS_VERSION 1




// Keyboard
static const uint8_t desc_rpt_kbd[] = {
    DESC_RPT_KBD
};
const usb_descriptor_t g_desc_rpt_kbd = { .buf = desc_rpt_kbd, .len = sizeof(desc_rpt_kbd) };


// Upper computer control center
#if !HAL_USB_CC_WINUSB_ENABLE
static const uint8_t desc_rpt_cc[] = {
  DESC_RPT_CC
};
const usb_descriptor_t g_desc_rpt_cc = { .buf = desc_rpt_cc, .len = sizeof(desc_rpt_cc) };
#endif

// Lighting
static const uint8_t desc_rpt_lighting[] = {
    DESC_RPT_LAMP
};
const usb_descriptor_t g_desc_rpt_lighting = { .buf = desc_rpt_lighting, .len = sizeof(desc_rpt_lighting) };

// Diagnostic Tracking
#if !HAL_USB_DT_WINUSB_ENABLE
static const uint8_t desc_rpt_dt[] = {
    DESC_RPT_DT
};
const usb_descriptor_t g_desc_rpt_dt = { .buf = desc_rpt_dt, .len = sizeof(desc_rpt_dt) };
#endif

// Gamepad standard hid
static const uint8_t desc_rpt_gamepad[] = {
    DESC_RPT_GAMEPAD
};
const usb_descriptor_t g_desc_rpt_gamepad = { .buf = desc_rpt_gamepad, .len = sizeof(desc_rpt_gamepad) };



// Configuration + interface ...... off
static const uint8_t config_descriptor_go[] = {
    CONFIG_DESC_INIT(0x04),
    CONFIG_DESC_KBD,
    CONFIG_DESC_CC,
    CONFIG_DESC_LIGHTING,
    CONFIG_DESC_DT
};
// Configuration + interface ...... standard hid
static const uint8_t config_descriptor_gh[] = {
    CONFIG_DESC_INIT(0x05),
    CONFIG_DESC_KBD,
    CONFIG_DESC_CC,
    CONFIG_DESC_LIGHTING,
    CONFIG_DESC_DT,
    CONFIG_DESC_GAMEPAD_HID
};
// Configuration + interface ...... xbox
static const uint8_t config_descriptor_gx[] = {
    CONFIG_DESC_INIT(0x05),
    CONFIG_DESC_KBD,
    CONFIG_DESC_CC,
    CONFIG_DESC_LIGHTING,
    CONFIG_DESC_DT,
    CONFIG_DESC_GAMEPAD_XBOX
};





/* ---- MS OS 2.0 描述符集：按开关拼装 ----
 * 结构（与已验证的 RAY40 工程一致）：
 *   Set Header(10) + Configuration Subset Header(8)
 *   + 每个 WinUSB 功能：Function Subset Header(8) + 兼容 ID(20) + DeviceInterfaceGUIDs(132) = 160
 *   + Xbox 模式追加：Function Subset Header(8) + 兼容 ID(20) = 28
 * Configuration Subset Header 不能省：缺了它 Windows 只会完整解析第一段，
 * 末尾的 XUSB10 兼容 ID 会被忽略，xusb22.sys 就不会绑定手柄接口。
 * 不用 SDK 的 USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_INIT()：
 * 它把 DeviceInterfaceGUIDs 写死成 {CDB3B5AD-293B-4663-AA36-1AAE46463776}（CMSIS-DAP 的 GUID），
 * 会让 J-Link Configurator 把本设备识别成 "3rd party CMSIS-DAP"。这里用我们自己的 GUID。
 * 注意：改 GUID 时需同步修改 wPropertyDataLength、wTotalLength 和 wSubsetLength。 */
static const uint8_t _msos_v2_0[] = {
    MSOS_V2_0_DESC,
#if HAL_USB_CC_WINUSB_ENABLE
    WINUSB_CC_MSOS20_DESC,
#endif
#if HAL_USB_DT_WINUSB_ENABLE
    WINUSB_DT_MSOS20_DESC
#endif
};

/* 编译期自检：数组长度必须与 MSOS20_SET_LEN 一致（开关或 GUID 改动导致长度漂移会直接报错） */
_Static_assert(sizeof(_msos_v2_0) == MSOS20_SET_LEN, "MS OS 2.0 descriptor length mismatch");


/* ---- Xbox 模式追加的函数子集 ----
 * 只含 Function Subset Header(8) + Compatible ID(20)，CompatibleID = "XUSB10"。
 * Windows 读到它就会给该接口加载 xusb22.sys（XInput），与 VID/PID 无关；
 * 缺了它设备管理器里只会有个无驱动接口，joy.cpl 里看不到手柄。
 * wSubsetLength 必须等于本子集总长（8 + 20 = 28）。 */
static const uint8_t _msos_v2_0_xbox_suffix[] = {
    XBOX_MSOS20_DESC
};


/* Xbox 模式下把基础描述符集 + Xbox 子集拼到这块缓冲里返回（wTotalLength 需同步回填） */
static uint8_t _msos_v2_0_xbox_buf[MSOS20_SET_LEN_XBOX];



static const uint8_t _desc_bos[] = {
    BOS_DESC
};
/* Xbox 模式的 BOS：唯一区别是 MS OS 2.0 描述符集长度多了 Xbox 子集 */
static const uint8_t _desc_bos_xbox[] = {
    BOS_XBOX_DESC
};
static struct usb_bos_descriptor bos_desc = {
    .string = _desc_bos,
    .string_len = sizeof(_desc_bos), 
};


/* 注意：不能再注册 MS OS 1.0 描述符。
 * cherryusb 的 usbd_vendor_request_handler() 里 msosv1 / msosv2 是 if-else 关系：
 * 两者用同一个 vendor code 时，Windows 发来的 MS OS 2.0 描述符集请求（wIndex=0x07）
 * 会先命中 MSOS 1.0 分支，落到 default 后被 STALL，导致枚举失败（设备管理器代码 43）。 */

static struct usb_msosv2_descriptor msosv2_desc = {
    .compat_id = _msos_v2_0,
    .compat_id_len = sizeof(_msos_v2_0),
    .vendor_code = MSOS_VENDOR_CODE,
};

#if HAL_USB_WEBUSB_ENABLE
// WebUSB：URL 描述符由厂商请求返回（cherryusb 在 bRequest = WEBUSB_VENDOR_CODE 时下发）
/* 把下面这串字符换成你自己的站点（不带 https://），浏览器会用它作为访问入口的跳转地址。
 * 长度由 sizeof 自动算出，改字符串不需要同步改长度。 */
static const uint8_t _webusb_url_desc[] = {
    WEB_URL_DESC
};
static struct usb_webusb_descriptor webusb_url_desc = {
    .vendor_code = WEBUSB_VENDOR_CODE,
    .string = (uint8_t *)_webusb_url_desc,
    .string_len = WEBUSB_URL_DESC_LEN,
};
#endif /* HAL_USB_WEBUSB_ENABLE */

/* 按当前手柄模式选择 BOS 与 MS OS 2.0 描述符集。
 * Xbox 模式多挂一个 XUSB10 函数子集，Windows 才会给手柄接口加载 xusb22.sys（与 VID/PID 无关）。
 * 必须在 usbd_desc_register() 之前调用；hal_usb_init() 每次重新注册描述符前都会调一次。 */
void hid_descriptor_init(void) {
    if (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) {
        uint16_t len = (uint16_t)(sizeof(_msos_v2_0) + sizeof(_msos_v2_0_xbox_suffix));

        memcpy(_msos_v2_0_xbox_buf, _msos_v2_0, sizeof(_msos_v2_0));
        memcpy(&_msos_v2_0_xbox_buf[sizeof(_msos_v2_0)], _msos_v2_0_xbox_suffix,
               sizeof(_msos_v2_0_xbox_suffix));

        /* 追加子集后要同步两处长度：
         *   Set Header 的 wTotalLength         @ 8..9
         *   Config Subset 的 wTotalLength      @ 16..17（= Set Header 之后的长度） */
        _msos_v2_0_xbox_buf[8] = (uint8_t)(len & 0xFFU);
        _msos_v2_0_xbox_buf[9] = (uint8_t)(len >> 8);
        _msos_v2_0_xbox_buf[16] = (uint8_t)((len - 10U) & 0xFFU);
        _msos_v2_0_xbox_buf[17] = (uint8_t)((len - 10U) >> 8);

        msosv2_desc.compat_id = _msos_v2_0_xbox_buf;
        msosv2_desc.compat_id_len = len;
        bos_desc.string = _desc_bos_xbox;
        bos_desc.string_len = sizeof(_desc_bos_xbox);
    } 
    else {
        msosv2_desc.compat_id = _msos_v2_0;
        msosv2_desc.compat_id_len = sizeof(_msos_v2_0);
        bos_desc.string = _desc_bos;
        bos_desc.string_len = sizeof(_desc_bos);
    }
}

static uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, USB_PRODUCT_VERSION, 0x01)
};
static const uint8_t *device_descriptor_callback(uint8_t speed) {
    (void)speed;

    uint8_t *buf = device_descriptor;

    uint16_t bcdDevice = USB_PRODUCT_VERSION;
    uint16_t bcdUSB    = (HAL_USB_WINUSB_ENABLE || (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX)) ? USB_2_1 : USB_2_0;

    // bcdUSB, bcdDevice
    buf[2]  = (uint8_t)(bcdUSB & 0xFF);
    buf[3]  = (uint8_t)(bcdUSB >> 8);
    buf[12] = (uint8_t)(bcdDevice & 0xFF);
    buf[13] = (uint8_t)(bcdDevice >> 8);

    return buf;
}

// Configuration + interface ...... off
static uint8_t config_descriptor_buf[255] = {0x00
    // CONFIG_DESC_INIT(0x04),
    // CONFIG_DESC_KBD,
    // CONFIG_DESC_CC,
    // CONFIG_DESC_LIGHTING,
    // CONFIG_DESC_DT
};
static const uint8_t *config_descriptor_callback(uint8_t speed) {
    (void)speed;
    const uint8_t *selected;
    uint16_t len;
    uint8_t *buf    = config_descriptor_buf;
    uint8_t buf_len = sizeof(config_descriptor_buf);
    switch (g_gamepad_mode) {
        case HAL_GAMEPAD_MODE_HID:
            selected = config_descriptor_gh;
            len = sizeof(config_descriptor_gh);
            break;
        case HAL_GAMEPAD_MODE_XBOX:
            selected = config_descriptor_gx;
            len = sizeof(config_descriptor_gx);
            break;
        default:
            selected = config_descriptor_go;
            len = sizeof(config_descriptor_go);
            break;
    }

    if (len > buf_len) {
        USB_LOG_ERR("config descriptor buf overflow\r\n");
        return NULL;
    }

    memcpy(buf, selected, len);
    buf[2] = (uint8_t)(len & 0xFF);
    buf[3] = (uint8_t)(len >> 8);

    USB_LOG_RAW("[usb] config desc: %u intf, %u bytes, Diagnostic Tracking bulk mps=%u\r\n",
                buf[4], (unsigned)len, (unsigned)INTF_DT_EP_SIZE);

    return buf;
}


// Device Quality Descriptor
static uint8_t device_quality_descriptor_usb[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, 0x01),
};
static const uint8_t *device_quality_descriptor_callback(uint8_t speed) {
    (void)speed;
    uint8_t *buf    = device_quality_descriptor_usb;
    uint16_t bcdUSB = (HAL_USB_WINUSB_ENABLE || (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX)) ? USB_2_1 : USB_2_0;

    buf[2] = (uint8_t)(bcdUSB & 0xFF);
    buf[3] = (uint8_t)(bcdUSB >> 8);
    
    return buf;
}

// Other Speed Descriptor
static uint8_t other_speed_descriptor_buf[255];
static const uint8_t *other_speed_descriptor_callback(uint8_t speed) {
    const uint8_t *cur = config_descriptor_callback(speed);
    uint16_t len       = (uint16_t)(cur[2] | (cur[3] << 8));
    uint8_t *buf       = other_speed_descriptor_buf;
    uint8_t buf_len    = sizeof(other_speed_descriptor_buf);

    if (len > buf_len) {
        len = buf_len;
    }
    memcpy(buf, cur, len);
    buf[1] = USB_DESCRIPTOR_TYPE_OTHER_SPEED;
    return buf;
}

// String
static char const *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },           // 0: English (0x0409)
    "Ray",                                  // 1: Manufacturer
    "Trace40",                              // 2: Product
    "TRACE40-0001",                         // 3: Serial (usb_get_serial() generate)

    "Ray Keyboard/Mouse/Consumer/System",   // 4: ITF KBD (Keyboard+Mouse+Consumer+System)
    "Ray Control Center",                   // 5: ITF Custom (Control Center)
    "Ray Dynamic Lighting",                 // 6: ITF Lighting (Dynamic Lighting)
    "Ray Diagnostic Tracking",              // 7: ITF Diagnostic (Diagnostic Tracking)
    "Ray Gamepad HID",                      // 8: ITF Gamepad (Standard HID)
    "Ray Xbox 360 Controller for Windows",  // 9: ITF Gamepad (Xbox)
};
static const char *string_descriptor_callback(uint8_t speed, uint8_t index) {
    (void)speed;
    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor g_hid_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .other_speed_descriptor_callback = other_speed_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,

    // .msosv1_descriptor = &msosv1_desc,
    .msosv2_descriptor = &msosv2_desc,
#if HAL_USB_WEBUSB_ENABLE
    .webusb_url_descriptor = &webusb_url_desc,
#endif
    .bos_descriptor = &bos_desc,
};



