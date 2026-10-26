#include "hid_descriptor.h"
#include "hid_report_desc.h"

#include "usbd_core.h"
#include "usbd_hid.h"
#include "tu_hid_device.h"


#define USE_MSOS_VERSION 1


// 配置描述符总长：获取阶段会自动算出长度
#define CONFIG_DESC_SIZ         (73)


// Configuration descriptor
#define  CONFIG_DESC_INIT(num_intf) \
    USB_CONFIG_DESCRIPTOR_INIT(CONFIG_DESC_SIZ, (num_intf), 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER)


// Keyboard
#define  CONFIG_DESC_KBD \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_KBD_NUM, 0x00, 0x02, USB_DEVICE_CLASS_HID, 0x01, 0x01, STRID_INTF_KBD), \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_SIZ_KBD), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_KBD_EPI, 0x03, INTF_KBD_EPI_SIZE, INTF_KBD_EPI_INTERVAL), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_KBD_EPO, 0x03, INTF_KBD_EPO_SIZE, INTF_KBD_EPO_INTERVAL)


// Upper computer control center
#if HAL_USB_CC_WINUSB_ENABLE
#define  CONFIG_DESC_CC \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_CC_NUM, 0x00, 0x02, USB_DEVICE_CLASS_VEND_SPECIFIC, 0x00, 0x00, STRID_INTF_CUSTOM), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_CC_EPI, 0x03, INTF_CC_EP_SIZE, INTF_CC_EP_INTERVAL), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_CC_EPO, 0x03, INTF_CC_EP_SIZE, INTF_CC_EP_INTERVAL)
#else
#define  CONFIG_DESC_CC \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_CC_NUM, 0x00, 0x02, USB_DEVICE_CLASS_HID, 0x00, 0x00, STRID_INTF_CUSTOM), \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_SIZ_CC), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_CC_EPI, 0x03, INTF_CC_EP_SIZE, INTF_CC_EP_INTERVAL), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_CC_EPO, 0x03, INTF_CC_EP_SIZE, INTF_CC_EP_INTERVAL)
#endif


// lighting
#define  CONFIG_DESC_LIGHTING \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_LIGHTING_NUM, 0x00, 0x01, USB_DEVICE_CLASS_HID, 0x00, 0x00, STRID_INTF_LIGHTING), \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_SIZ_LIGHTING), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_LIGHTING_EPI, 0x03, INTF_LIGHTING_EPI_SIZE, INTF_LIGHTING_EPI_INTERVAL)
    // USB_ENDPOINT_DESCRIPTOR_INIT(INTF_LIGHTING_EPO, 0x03, INTF_LIGHTING_EPO_SIZE, INTF_LIGHTING_EPO_INTERVAL)


// Diagnostic Tracking
#if HAL_USB_DT_WINUSB_ENABLE
#define  CONFIG_DESC_DT \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_DT_NUM, 0x00, 0x02, USB_DEVICE_CLASS_VEND_SPECIFIC, 0x00, 0x00, STRID_INTF_DT), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_DT_EPI, INTF_DT_EP_TYPE, INTF_DT_EP_SIZE, INTF_DT_EPI_INTERVAL), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_DT_EPO, INTF_DT_EP_TYPE, INTF_DT_EP_SIZE, INTF_DT_EPO_INTERVAL)
#else
#define  CONFIG_DESC_DT \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_DT_NUM, 0x00, 0x02, USB_DEVICE_CLASS_HID, 0x00, 0x00, STRID_INTF_DT), \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_SIZ_DT), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_DT_EPI, INTF_DT_EP_TYPE, INTF_DT_EP_SIZE, INTF_DT_EPI_INTERVAL), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_DT_EPO, INTF_DT_EP_TYPE, INTF_DT_EP_SIZE, INTF_DT_EPO_INTERVAL)
#endif

// gamepad hid
#define  CONFIG_DESC_GAMEPAD_HID \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_GAMEPAD_NUM, 0x00, 0x01, USB_DEVICE_CLASS_HID, 0x00, 0x00, STRID_INTF_GAMEPAD), \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_SIZ_GAMEPAD), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_GAMEPAD_HID_EPI, 0x03, INTF_GAMEPAD_HID_EPI_SIZE, INTF_GAMEPAD_HID_EPI_INTERVAL)
    // USB_ENDPOINT_DESCRIPTOR_INIT(INTF_GAMEPAD_HID_EPO, 0x03, INTF_GAMEPAD_HID_EPO_SIZE, INTF_GAMEPAD_HID_EPO_INTERVAL)

/** Microsoft Xbox 描述符
 *  xusb22.sys 解析此描述符获取 IN/OUT 端点地址 + 报告大小。
 *  缺失 → 驱动加载但无法打开端点 → 设备管理器可见但 joy.cpl 不可见。
 *  伪装成 HID descriptor（bNumDescriptors）
 */ 
#define USB_MICROSOFT_XBOX_DESCRIPTOR_INIT(bEndpointInAddress, wInMaxPacketSize, bEndpointOutAddress, wOutMaxPacketSize)\
    0x11,                         /* bLength */                                             \
    0x21,                         /* bDescriptorType = Microsoft Xbox */                    \
    0x00, 0x01,                   /* bcdHID = 0x0100 */                                     \
    0x01,                         /* bCountryCode */                                        \
    0x25,                         /* bNumDescriptors (Microsoft special value) */           \
    bEndpointInAddress,           /* bEndpointAddress */                                    \
    WBVAL(wInMaxPacketSize),      /* wMaxPacketSize = 20 */                                 \
    0x00,                                                                                   \
    0x00, 0x00,                                                                             \
    0x13,                                                                                   \
    bEndpointOutAddress,          /* bEndpointAddress */                                    \
    WBVAL(wOutMaxPacketSize),     /* wMaxPacketSize = 8 */                                  \
    0x00


// gamepad xbox
#define  CONFIG_DESC_GAMEPAD_XBOX \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_GAMEPAD_NUM, 0x00, 0x02, USB_DEVICE_CLASS_VEND_SPECIFIC, 0x5D, 0x01, STRID_INTF_GAMEPAD), \
    USB_MICROSOFT_XBOX_DESCRIPTOR_INIT(INTF_GAMEPAD_XBOX_EPI, INTF_GAMEPAD_XBOX_RPT_LEN, INTF_GAMEPAD_XBOX_EPO, INTF_GAMEPAD_XBOX_OUT_RPT_LEN), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_GAMEPAD_XBOX_EPI, 0x03, INTF_GAMEPAD_XBOX_EPI_SIZE, INTF_GAMEPAD_XBOX_EPI_INTERVAL), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_GAMEPAD_XBOX_EPO, 0x03, INTF_GAMEPAD_XBOX_EPO_SIZE, INTF_GAMEPAD_XBOX_EPO_INTERVAL)



// Keyboard
#define DESC_RPT_SIZ_KBD             (sizeof(desc_rpt_kbd))
static const uint8_t desc_rpt_kbd[] = {
    REPORT_DESC_KBD
};
const usb_descriptor_t g_desc_rpt_kbd = { .buf = desc_rpt_kbd, .len = sizeof(desc_rpt_kbd) };


// Upper computer control center
#if !HAL_USB_CC_WINUSB_ENABLE
#define DESC_RPT_SIZ_CC              (sizeof(desc_rpt_cc))
static const uint8_t desc_rpt_cc[] = {
  TUD_HID_REPORT_DESC_GENERIC_INOUT(INTF_CC_EP_SIZE)
};
const usb_descriptor_t g_desc_rpt_cc = { .buf = desc_rpt_cc, .len = sizeof(desc_rpt_cc) };
#endif

// Lighting
#define DESC_RPT_SIZ_LIGHTING        (sizeof(desc_rpt_lighting))
static const uint8_t desc_rpt_lighting[] = {
  TUD_HID_REPORT_DESC_LIGHTING(LIGHTING_REPORT_ID_ATTRIBUTES)
};
const usb_descriptor_t g_desc_rpt_lighting = { .buf = desc_rpt_lighting, .len = sizeof(desc_rpt_lighting) };

// Diagnostic Tracking
#if !HAL_USB_DT_WINUSB_ENABLE
#define DESC_RPT_SIZ_DT              (sizeof(desc_rpt_dt))
static const uint8_t desc_rpt_dt[] = {
  TUD_HID_REPORT_DESC_GENERIC_INOUT(INTF_DT_EP_SIZE)
};
const usb_descriptor_t g_desc_rpt_dt = { .buf = desc_rpt_dt, .len = sizeof(desc_rpt_dt) };
#endif

// Gamepad standard hid
#define DESC_RPT_SIZ_GAMEPAD    (sizeof(desc_rpt_gamepad))
static const uint8_t desc_rpt_gamepad[] = {
    REPORT_DESC_GAMEPAD
};
const usb_descriptor_t g_desc_rpt_gamepad = { .buf = desc_rpt_gamepad, .len = sizeof(desc_rpt_gamepad) };


// Device
/* bcdUSB 必须 >= 0x0210，Windows 才会去读 BOS 里的 MS OS 2.0 / WebUSB 描述符。 */
static const uint8_t device_descriptor[] = {
#if HAL_USB_WINUSB_ENABLE
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_1, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0002, 0x01)
#else
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0002, 0x01)
#endif
};

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





#if HAL_USB_WINUSB_ENABLE
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
#define MSOS20_CONFIG_SUBSET_LEN  (8U)
#define MSOS20_SET_LEN   (10U + MSOS20_CONFIG_SUBSET_LEN \
                          + (HAL_USB_CC_WINUSB_ENABLE ? USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN : 0U) \
                          + (HAL_USB_DT_WINUSB_ENABLE ? USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN : 0U))

static const uint8_t _msos_v2_0[] = {
    /* ---- Set Header (10) ---- */
    USB_MSOSV2_COMP_ID_SET_HEADER_DESCRIPTOR_INIT(MSOS20_SET_LEN),

    /* ---- Configuration Subset Header (8) ---- */
    WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE),      /* wLength */
    WBVAL(WINUSB_SUBSET_HEADER_CONFIGURATION_TYPE), /* MS_OS_20_SUBSET_HEADER_CONFIGURATION */
    0x00,                                           /* bConfigurationIndex */
    0x00,                                           /* bReserved */
    WBVAL(MSOS20_SET_LEN - 10U),                    /* wTotalLength = Set Header 之后的长度 */

#if HAL_USB_CC_WINUSB_ENABLE
    /* ==== Function Subset #1：interface 1 = control center（上位机）==== */
    WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE), /* wLength */
    WBVAL(WINUSB_SUBSET_HEADER_FUNCTION_TYPE), /* MS_OS_20_SUBSET_HEADER_FUNCTION */
    INTF_CC_NUM,                               /* bFirstInterface */
    0x00,                                      /* bReserved */
    WBVAL(USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN), /* wSubsetLength = 8 + 20 + 132 */

    /* Compatible ID = "WINUSB" (20) */
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_SIZE), /* wLength */
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_TYPE), /* MS_OS_20_FEATURE_COMPATBLE_ID */
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0,       /* CompatibleID */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* SubCompatibleID */

    /* 注册表属性 DeviceInterfaceGUIDs = HAL_USB_CC_GUID (132) */
    WBVAL(132),                                /* wLength */
    WBVAL(WINUSB_FEATURE_REG_PROPERTY_TYPE),   /* MS_OS_20_FEATURE_REG_PROPERTY */
    WBVAL(WINUSB_PROP_DATA_TYPE_REG_MULTI_SZ), /* REG_MULTI_SZ */
    WBVAL(42),                                 /* wPropertyNameLength */
    'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,
    'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,
    WBVAL(80),                                 /* wPropertyDataLength */
    '{', 0,
    '1', 0, 'F', 0, '3', 0, 'C', 0, '7', 0, 'A', 0, '2', 0, '1', 0, '-', 0,
    '8', 0, 'B', 0, '4', 0, 'E', 0, '-', 0,
    '4', 0, 'D', 0, '9', 0, '5', 0, '-', 0,
    'A', 0, '6', 0, 'C', 0, '1', 0, '-', 0,
    '2', 0, 'E', 0, '7', 0, 'B', 0, '9', 0, 'F', 0, '4', 0, 'D', 0, '5', 0, 'A', 0, '3', 0, '8', 0,
    '}', 0, 0, 0, 0, 0,
#endif

#if HAL_USB_DT_WINUSB_ENABLE
    /* ==== Function Subset #2：interface 3 = diagnostic tracking ==== */
    /* ---- Function Subset Header (8)：bFirstInterface = diagnostic tracking ---- */
    WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE),
    WBVAL(WINUSB_SUBSET_HEADER_FUNCTION_TYPE),
    INTF_DT_NUM,
    0x00,
    WBVAL(USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN),

    /* ---- Compatible ID = "WINUSB" (20) ---- */
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_SIZE),
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_TYPE),
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    /* ---- 注册表属性 DeviceInterfaceGUIDs = HAL_USB_DT_GUID (132) ---- */
    WBVAL(132),
    WBVAL(WINUSB_FEATURE_REG_PROPERTY_TYPE),
    WBVAL(WINUSB_PROP_DATA_TYPE_REG_MULTI_SZ),
    WBVAL(42),
    'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,
    'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,
    WBVAL(80),
    '{', 0,
    '6', 0, 'B', 0, '1', 0, 'D', 0, '7', 0, 'C', 0, '3', 0, 'E', 0, '-', 0,
    '4', 0, 'A', 0, '2', 0, 'F', 0, '-', 0,
    '4', 0, 'E', 0, '8', 0, 'B', 0, '-', 0,
    '9', 0, 'C', 0, '5', 0, 'D', 0, '-', 0,
    '1', 0, 'F', 0, '0', 0, 'A', 0, '2', 0, 'B', 0, '3', 0, 'C', 0, '4', 0, 'D', 0, '5', 0, 'E', 0,
    '}', 0, 0, 0, 0, 0
#endif
};

/* 编译期自检：数组长度必须与 MSOS20_SET_LEN 一致（开关或 GUID 改动导致长度漂移会直接报错） */
typedef char msos20_set_len_check[(sizeof(_msos_v2_0) == MSOS20_SET_LEN) ? 1 : -1];


/* ---- Xbox 模式追加的函数子集 ----
 * 只含 Function Subset Header(8) + Compatible ID(20)，CompatibleID = "XUSB10"。
 * Windows 读到它就会给该接口加载 xusb22.sys（XInput），与 VID/PID 无关；
 * 缺了它设备管理器里只会有个无驱动接口，joy.cpl 里看不到手柄。
 * wSubsetLength 必须等于本子集总长（8 + 20 = 28）。 */
#define MSOS20_XBOX_SUBSET_LEN  (WINUSB_FUNCTION_SUBSET_HEADER_SIZE + WINUSB_FEATURE_COMPATIBLE_ID_SIZE)
#define MSOS20_SET_LEN_XBOX     (MSOS20_SET_LEN + MSOS20_XBOX_SUBSET_LEN)

static const uint8_t _msos_v2_0_xbox_suffix[MSOS20_XBOX_SUBSET_LEN] = {
    WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE), /* wLength */
    WBVAL(WINUSB_SUBSET_HEADER_FUNCTION_TYPE), /* MS_OS_20_SUBSET_HEADER_FUNCTION */
    INTF_GAMEPAD_NUM,                          /* bFirstInterface = gamepad */
    0x00,                                      /* bReserved */
    WBVAL(MSOS20_XBOX_SUBSET_LEN),             /* wSubsetLength */

    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_SIZE),  /* wLength */
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_TYPE),  /* MS_OS_20_FEATURE_COMPATBLE_ID */
    'X', 'U', 'S', 'B', '1', '0', 0, 0,        /* CompatibleID = "XUSB10" */
    0, 0, 0, 0, 0, 0, 0, 0,                    /* SubCompatibleID */
};

/* Xbox 模式下把基础描述符集 + Xbox 子集拼到这块缓冲里返回（wTotalLength 需同步回填） */
static uint8_t _msos_v2_0_xbox_buf[MSOS20_SET_LEN_XBOX];



// BOS：WinUSB（MS OS 2.0）平台能力，WebUSB 由 HAL_USB_WEBUSB_ENABLE 控制
#if HAL_USB_WEBUSB_ENABLE
    #define MSOS20_BOS_TOTAL_LEN  (5 + USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_LEN + USB_BOS_CAP_PLATFORM_WEBUSB_DESCRIPTOR_LEN)
    #define MSOS20_BOS_NUM_CAPS   (2)
#else
    #define MSOS20_BOS_TOTAL_LEN  (5 + USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_LEN)
    #define MSOS20_BOS_NUM_CAPS   (1)
#endif

static const uint8_t _desc_bos[] = {
    USB_BOS_HEADER_DESCRIPTOR_INIT(MSOS20_BOS_TOTAL_LEN, MSOS20_BOS_NUM_CAPS),
    USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_INIT(MSOS_VENDOR_CODE, sizeof(_msos_v2_0)),
#if HAL_USB_WEBUSB_ENABLE
    USB_BOS_CAP_PLATFORM_WEBUSB_DESCRIPTOR_INIT(WEBUSB_VENDOR_CODE, 0x01),
#endif
};
/* Xbox 模式的 BOS：唯一区别是 MS OS 2.0 描述符集长度多了 Xbox 子集 */
static const uint8_t _desc_bos_xbox[] = {
    USB_BOS_HEADER_DESCRIPTOR_INIT(MSOS20_BOS_TOTAL_LEN, MSOS20_BOS_NUM_CAPS),
    USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_INIT(MSOS_VENDOR_CODE, MSOS20_SET_LEN_XBOX),
#if HAL_USB_WEBUSB_ENABLE
    USB_BOS_CAP_PLATFORM_WEBUSB_DESCRIPTOR_INIT(WEBUSB_VENDOR_CODE, 0x01),
#endif
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
#define WEBUSB_URL_CHARS \
    'r', 'a', 'y', '.', 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm', '/', 'w', 'e', 'b', 'u', 's', 'b'
#define WEBUSB_URL_LEN        (sizeof((const char[]){ WEBUSB_URL_CHARS })) /* 花括号字符表不带结尾 0 */
#define WEBUSB_URL_DESC_LEN   (3U + WEBUSB_URL_LEN) /* bLength + bDescriptorType + bScheme */

static const uint8_t _webusb_url_desc[] = {
    (uint8_t)WEBUSB_URL_DESC_LEN,
    WEBUSB_URL_TYPE,         /* bDescriptorType = URL 描述符 */
    WEBUSB_URL_SCHEME_HTTPS, /* bScheme：https */
    WEBUSB_URL_CHARS         /* URL 本身不带 scheme */
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
void hid_descriptor_apply_gamepad_mode(void)
{
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
    } else {
        msosv2_desc.compat_id = _msos_v2_0;
        msosv2_desc.compat_id_len = sizeof(_msos_v2_0);
        bos_desc.string = _desc_bos;
        bos_desc.string_len = sizeof(_desc_bos);
    }
}

#endif /* HAL_USB_WINUSB_ENABLE */

#if !HAL_USB_WINUSB_ENABLE
/* 没有 WinUSB 功能时既不需要 BOS 也不需要 MS OS 2.0，模式切换不改变描述符 */
void hid_descriptor_apply_gamepad_mode(void)
{
}
#endif



static const uint8_t *device_descriptor_callback(uint8_t speed) {
    (void)speed;
// #if USE_MSOS_VERSION == 2
//     return (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) ? device_descriptor_msos : device_descriptor;
// #else
    return device_descriptor;
// #endif
}

// 三种配置描述符的接口数量不同，wTotalLength 也不同。统一拷贝到缓冲区后，按实际数组长度回填 wTotalLength
static uint8_t config_descriptor_buf[255];
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
static const uint8_t device_quality_descriptor[] = {
    #if HAL_USB_WINUSB_ENABLE
        USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_1, 0x00, 0x00, 0x00, 0x01),
    #else
        USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, 0x01),
    #endif
};
static const uint8_t *device_quality_descriptor_callback(uint8_t speed) {
    (void)speed;
    return device_quality_descriptor;
}

// Other Speed Descriptor
static uint8_t other_speed_descriptor_buf[255];
static const uint8_t *other_speed_descriptor_callback(uint8_t speed) {
    const uint8_t *cur = config_descriptor_callback(speed);
    uint16_t len       = (uint16_t)(cur[2] | (cur[3] << 8));
    uint8_t *buf       = other_speed_descriptor_buf;
    uint8_t buf_len    = sizeof(other_speed_descriptor_buf);

    if (len > buf_len)
    {
        len = buf_len;
    }
    memcpy(buf, cur, len);
    buf[1] = USB_DESCRIPTOR_TYPE_OTHER_SPEED;
    return buf;
}

/* 不再提供 Device Qualifier 和 Other Speed Configuration：
 * 本设备只跑一种速度，返回 qualifier 会让主机去读 OTHER_SPEED_CONFIG，
 * 而它只能由全速配置复制而来，作为高速配置校验时内容非法（如 bulk 端点 mps 必须为 512），
 * 会让 Windows 报 CONFIGURATION_DESCRIPTOR_VALIDATION_FAILURE。
 * 回调置空后 cherryusb 会直接 STALL 这两个请求，符合单速设备的行为。 */


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
  "Ray Gamepad",                          // 8: ITF Gamepad (XBOX 360 / HID 模式共用)
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

#if HAL_USB_WINUSB_ENABLE
    // .msosv1_descriptor = &msosv1_desc,
    .msosv2_descriptor = &msosv2_desc,
#if HAL_USB_WEBUSB_ENABLE
    .webusb_url_descriptor = &webusb_url_desc,
#endif
    .bos_descriptor = &bos_desc,
#endif
};



