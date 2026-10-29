#ifndef KBD_DESCRIPTOR_H
#define KBD_DESCRIPTOR_H

#include "usbd_core.h"
#include "usbd_hid.h"

#include "hid_descriptor.h"

/**
 * USB 描述符拓扑
 *
 * Device Descriptor
 *   `-- Configuration Descriptor (设备当前只返回以下一种配置)
 *         |-- Interface 0：Keyboard (HID)
 *         |     |-- HID Descriptor -> Keyboard Report Descriptor (独立请求)
 *         |     |-- Endpoint IN
 *         |     `-- Endpoint OUT
 *         |-- Interface 1：Control Center (HID OR WinUSB)
 *         |     |-- HID Descriptor -> Control Center Report Descriptor (HID MODE)
 *         |     |-- Endpoint IN
 *         |     `-- Endpoint OUT
 *         |-- Interface 2：Lighting (HID)
 *         |     |-- HID Descriptor -> Lighting Report Descriptor (独立请求)
 *         |     `-- Endpoint IN
 *         |-- Interface 3：Diagnostic Tracking (HID OR WinUSB)
 *         |     |-- HID Descriptor -> Diagnostic Report Descriptor (HID MODE)
 *         |     |-- Endpoint IN
 *         |     `-- Endpoint OUT
 *         |-- Interface 4-1：Gamepad Standard HID (selectable)
 *         |     |-- HID Descriptor -> Gamepad Report Descriptor (HID MODE)
 *         |     `-- Endpoint IN
 *         `-- Interface 4-2：Gamepad XBOX 360 Controller for Windows  (selectable)
 *               |-- Microsoft Xbox Descriptor
 *               |-- Endpoint IN
 *               `-- Endpoint OUT
 *
 */

/** **************************************************** Device Descriptor **************************************************** */

/** **************************************************** Configuration Descriptor **************************************************** */
// 配置描述符总长：获取阶段会自动算出长度
#define CONFIG_DESC_SIZ         (73)
#define CONFIG_DESC_INIT(num_intf) USB_CONFIG_DESCRIPTOR_INIT(CONFIG_DESC_SIZ, (num_intf), 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER)

/** **************************************************** Interface Descriptor **************************************************** */
// Keyboard
#define CONFIG_DESC_KBD                                                                                                     \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_KBD_NUM, 0x00, 0x02, USB_DEVICE_CLASS_HID, 0x01, 0x01, STRID_INTF_KBD),              \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_KBD_SIZ),                                                      \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_KBD_EPI, 0x03, INTF_KBD_EPI_SIZE, INTF_KBD_EPI_INTERVAL),                             \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_KBD_EPO, 0x03, INTF_KBD_EPO_SIZE, INTF_KBD_EPO_INTERVAL)

// Upper computer control center
#if HAL_USB_CC_WINUSB_ENABLE
#define CONFIG_DESC_CC                                                                                                      \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_CC_NUM, 0x00, 0x02, USB_DEVICE_CLASS_VEND_SPECIFIC, 0x00, 0x00, STRID_INTF_CUSTOM),  \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_CC_EPI, 0x03, INTF_CC_EP_SIZE, INTF_CC_EP_INTERVAL),                                  \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_CC_EPO, 0x03, INTF_CC_EP_SIZE, INTF_CC_EP_INTERVAL)
#else
#define CONFIG_DESC_CC                                                                                                      \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_CC_NUM, 0x00, 0x02, USB_DEVICE_CLASS_HID, 0x00, 0x00, STRID_INTF_CUSTOM),            \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_CC_SIZ),                                                       \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_CC_EPI, 0x03, INTF_CC_EP_SIZE, INTF_CC_EP_INTERVAL),                                  \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_CC_EPO, 0x03, INTF_CC_EP_SIZE, INTF_CC_EP_INTERVAL)
#endif

// lighting
#define CONFIG_DESC_LIGHTING                                                                                                \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_LIGHTING_NUM, 0x00, 0x01, USB_DEVICE_CLASS_HID, 0x00, 0x00, STRID_INTF_LIGHTING),    \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_LAMP_SIZ),                                                     \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_LIGHTING_EPI, 0x03, INTF_LIGHTING_EPI_SIZE, INTF_LIGHTING_EPI_INTERVAL)
    // USB_ENDPOINT_DESCRIPTOR_INIT(INTF_LIGHTING_EPO, 0x03, INTF_LIGHTING_EPO_SIZE, INTF_LIGHTING_EPO_INTERVAL)


// Diagnostic Tracking
#if HAL_USB_DT_WINUSB_ENABLE
#define CONFIG_DESC_DT                                                                                                      \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_DT_NUM, 0x00, 0x02, USB_DEVICE_CLASS_VEND_SPECIFIC, 0x00, 0x00, STRID_INTF_DT),      \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_DT_EPI, INTF_DT_EP_TYPE, INTF_DT_EP_SIZE, INTF_DT_EPI_INTERVAL),                      \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_DT_EPO, INTF_DT_EP_TYPE, INTF_DT_EP_SIZE, INTF_DT_EPO_INTERVAL)
#else
#define CONFIG_DESC_DT                                                                                                      \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_DT_NUM, 0x00, 0x02, USB_DEVICE_CLASS_HID, 0x00, 0x00, STRID_INTF_DT),                \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_DT_SIZ),                                                       \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_DT_EPI, INTF_DT_EP_TYPE, INTF_DT_EP_SIZE, INTF_DT_EPI_INTERVAL),                      \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_DT_EPO, INTF_DT_EP_TYPE, INTF_DT_EP_SIZE, INTF_DT_EPO_INTERVAL)
#endif

// gamepad hid
#define CONFIG_DESC_GAMEPAD_HID                                                                                             \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_GAMEPAD_NUM, 0x00, 0x01, USB_DEVICE_CLASS_HID, 0x00, 0x00, STRID_INTF_GAMEPAD_HID),  \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, WBVAL(DESC_RPT_GAMEPAD_SIZ),                                                  \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_GAMEPAD_HID_EPI, 0x03, INTF_GAMEPAD_HID_EPI_SIZE, INTF_GAMEPAD_HID_EPI_INTERVAL)
    // USB_ENDPOINT_DESCRIPTOR_INIT(INTF_GAMEPAD_HID_EPO, 0x03, INTF_GAMEPAD_HID_EPO_SIZE, INTF_GAMEPAD_HID_EPO_INTERVAL)

/** Microsoft Xbox 描述符
 *  xusb22.sys 解析此描述符获取 IN/OUT 端点地址 + 报告大小。
 *  缺失 → 驱动加载但无法打开端点 → 设备管理器可见但 joy.cpl 不可见。
 *  伪装成 HID descriptor（bNumDescriptors）
 */ 
#define USB_MICROSOFT_XBOX_DESCRIPTOR_INIT(bEndpointInAddress, wInMaxPacketSize, bEndpointOutAddress, wOutMaxPacketSize) \
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
#define CONFIG_DESC_GAMEPAD_XBOX \
    USB_INTERFACE_DESCRIPTOR_INIT(INTF_GAMEPAD_NUM, 0x00, 0x02, USB_DEVICE_CLASS_VEND_SPECIFIC, 0x5D, 0x01, STRID_INTF_GAMEPAD_XBOX),           \
    USB_MICROSOFT_XBOX_DESCRIPTOR_INIT(INTF_GAMEPAD_XBOX_EPI, INTF_GAMEPAD_XBOX_RPT_LEN, INTF_GAMEPAD_XBOX_EPO, INTF_GAMEPAD_XBOX_OUT_RPT_LEN), \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_GAMEPAD_XBOX_EPI, 0x03, INTF_GAMEPAD_XBOX_EPI_SIZE, INTF_GAMEPAD_XBOX_EPI_INTERVAL),                      \
    USB_ENDPOINT_DESCRIPTOR_INIT(INTF_GAMEPAD_XBOX_EPO, 0x03, INTF_GAMEPAD_XBOX_EPO_SIZE, INTF_GAMEPAD_XBOX_EPO_INTERVAL)


/** **************************************************** Microsoft OS 2.0 and Bos Descriptor **************************************************** */
#if HAL_USB_CC_WINUSB_ENABLE
    #define WINUSB_CC_MSOS20_DESC                                                                   \
    /* ---- Function Subset Header (8): bFirstInterface = control center ---- */                    \
    WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE), /* wLength */                                        \
    WBVAL(WINUSB_SUBSET_HEADER_FUNCTION_TYPE), /* MS_OS_20_SUBSET_HEADER_FUNCTION */                \
    INTF_CC_NUM,                               /* bFirstInterface */                                \
    0x00,                                      /* bReserved */                                      \
    WBVAL(USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN),                                 \
    /* Compatible ID = "WINUSB" (20) */                                                             \
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_SIZE), /* wLength */                                         \
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_TYPE), /* MS_OS_20_FEATURE_COMPATBLE_ID */                   \
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0,       /* CompatibleID */                                    \
    0, 0, 0, 0, 0, 0, 0, 0,                   /* SubCompatibleID */                                 \
    /* Registry properties DeviceInterfaceGUIDs = HAL_USB_CC_GUID (132) */                          \
    WBVAL(132),                                /* wLength */                                        \
    WBVAL(WINUSB_FEATURE_REG_PROPERTY_TYPE),   /* MS_OS_20_FEATURE_REG_PROPERTY */                  \
    WBVAL(WINUSB_PROP_DATA_TYPE_REG_MULTI_SZ), /* REG_MULTI_SZ */                                   \
    WBVAL(42),                                 /* wPropertyNameLength */                            \
    'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,                                                 \
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,                         \
    'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,                                                   \
    WBVAL(80),                                 /* wPropertyDataLength */                            \
    '{', 0,                                                                                         \
    '1', 0, 'F', 0, '3', 0, 'C', 0, '7', 0, 'A', 0, '2', 0, '1', 0, '-', 0,                         \
    '8', 0, 'B', 0, '4', 0, 'E', 0, '-', 0,                                                         \
    '4', 0, 'D', 0, '9', 0, '5', 0, '-', 0,                                                         \
    'A', 0, '6', 0, 'C', 0, '1', 0, '-', 0,                                                         \
    '2', 0, 'E', 0, '7', 0, 'B', 0, '9', 0, 'F', 0, '4', 0, 'D', 0, '5', 0, 'A', 0, '3', 0, '8', 0, \
    '}', 0, 0, 0, 0, 0
#endif

#if HAL_USB_DT_WINUSB_ENABLE
    #define WINUSB_DT_MSOS20_DESC                                                                   \
    /* ---- Function Subset Header (8): bFirstInterface = diagnostic tracking ---- */               \
    WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE),                                                      \
    WBVAL(WINUSB_SUBSET_HEADER_FUNCTION_TYPE),                                                      \
    INTF_DT_NUM,                                                                                    \
    0x00,                                                                                           \
    WBVAL(USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN),                                 \
    /* ---- Compatible ID = "WINUSB" (20) ---- */                                                   \
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_SIZE),                                                       \
    WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_TYPE),                                                       \
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0,                                                             \
    0, 0, 0, 0, 0, 0, 0, 0,                                                                         \
    /* ---- Registry properties DeviceInterfaceGUIDs = HAL_USB_DT_GUID (132) ---- */                \
    WBVAL(132),                                                                                     \
    WBVAL(WINUSB_FEATURE_REG_PROPERTY_TYPE),                                                        \
    WBVAL(WINUSB_PROP_DATA_TYPE_REG_MULTI_SZ),                                                      \
    WBVAL(42),                                                                                      \
    'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,                                                 \
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,                         \
    'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,                                                   \
    WBVAL(80),                                                                                      \
    '{', 0,                                                                                         \
    '6', 0, 'B', 0, '1', 0, 'D', 0, '7', 0, 'C', 0, '3', 0, 'E', 0, '-', 0,                         \
    '4', 0, 'A', 0, '2', 0, 'F', 0, '-', 0,                                                         \
    '4', 0, 'E', 0, '8', 0, 'B', 0, '-', 0,                                                         \
    '9', 0, 'C', 0, '5', 0, 'D', 0, '-', 0,                                                         \
    '1', 0, 'F', 0, '0', 0, 'A', 0, '2', 0, 'B', 0, '3', 0, 'C', 0, '4', 0, 'D', 0, '5', 0, 'E', 0, \
    '}', 0, 0, 0, 0, 0
#endif

    #define MSOS20_XBOX_SUBSET_LEN  (WINUSB_FUNCTION_SUBSET_HEADER_SIZE + WINUSB_FEATURE_COMPATIBLE_ID_SIZE)
    #define XBOX_MSOS20_DESC                                                                           \
        /* ---- Function Subset Header (8): bFirstInterface = gamepad xbox ---- */                      \
        WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE), /* wLength */                                        \
        WBVAL(WINUSB_SUBSET_HEADER_FUNCTION_TYPE), /* MS_OS_20_SUBSET_HEADER_FUNCTION */                \
        INTF_GAMEPAD_NUM,                          /* bFirstInterface = gamepad */                      \
        0x00,                                      /* bReserved */                                      \
        WBVAL(MSOS20_XBOX_SUBSET_LEN),             /* wSubsetLength */                                  \
        WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_SIZE),  /* wLength */                                        \
        WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_TYPE),  /* MS_OS_20_FEATURE_COMPATBLE_ID */                  \
        'X', 'U', 'S', 'B', '1', '0', 0, 0,        /* CompatibleID = "XUSB10" */                        \
        0, 0, 0, 0, 0, 0, 0, 0                    /* SubCompatibleID */
    

    #define MSOS20_SET_LEN (WINUSB_DESCRIPTOR_SET_HEADER_SIZE + WINUSB_FUNCTION_SUBSET_HEADER_SIZE      \
            + (HAL_USB_CC_WINUSB_ENABLE ? USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN : 0)  \
            + (HAL_USB_DT_WINUSB_ENABLE ? USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN : 0))
    #define MSOS_V2_0_DESC                                                                              \
        /* ---- Set Header (10) ---- */                                                                 \
        USB_MSOSV2_COMP_ID_SET_HEADER_DESCRIPTOR_INIT(MSOS20_SET_LEN),                                  \
        /* ---- Configuration Subset Header (8) ---- */                                                 \
        WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE),      /* wLength */                                   \
        WBVAL(WINUSB_SUBSET_HEADER_CONFIGURATION_TYPE), /* MS_OS_20_SUBSET_HEADER_CONFIGURATION */      \
        0x00,                                           /* bConfigurationIndex */                       \
        0x00,                                           /* bReserved */                                 \
        WBVAL(MSOS20_SET_LEN - 10U)                     /* wTotalLength = length after Set Header */


    #define MSOS20_SET_LEN_XBOX     (MSOS20_SET_LEN + MSOS20_XBOX_SUBSET_LEN)


    #if (HAL_USB_WEBUSB_ENABLE == 0)
        #define MSOS20_BOS_TOTAL_LEN    (5 + USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_LEN)
        #define MSOS20_BOS_NUM_CAPS     (1)
        #define WEBUSB_DESC 
    #else
        #define MSOS20_BOS_TOTAL_LEN    (5 + USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_LEN + USB_BOS_CAP_PLATFORM_WEBUSB_DESCRIPTOR_LEN)
        #define MSOS20_BOS_NUM_CAPS     (2)
        #define WEBUSB_DESC             USB_BOS_CAP_PLATFORM_WEBUSB_DESCRIPTOR_INIT(WEBUSB_VENDOR_CODE, WEBUSB_LANDING_PAGE_CODE)\
        
        #define WEBUSB_URL_CHARS \
            'r', 'a', 'y', '.', 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm', '/', 'w', 'e', 'b', 'u', 's', 'b'
        #define WEBUSB_URL_LEN        (sizeof((const char[]){ WEBUSB_URL_CHARS })) /* 花括号字符表不带结尾 0 */
        #define WEBUSB_URL_DESC_LEN   (3U + WEBUSB_URL_LEN) /* bLength + bDescriptorType + bScheme */

        #define WEB_URL_DESC                                                                    \
            (uint8_t)WEBUSB_URL_DESC_LEN,                                                       \
            WEBUSB_URL_TYPE,         /* bDescriptorType = URL descriptor */                     \
            WEBUSB_URL_SCHEME_HTTPS, /* bScheme：https */                                       \
            WEBUSB_URL_CHARS         /* URL 本身不带 scheme */
        
    #endif
    

    #define BOS_DESC                                                                            \
        USB_BOS_HEADER_DESCRIPTOR_INIT(MSOS20_BOS_TOTAL_LEN, MSOS20_BOS_NUM_CAPS),              \
        USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_INIT(MSOS_VENDOR_CODE, MSOS20_SET_LEN)           \
        , WEBUSB_DESC

    #define BOS_XBOX_DESC                                                                       \
        USB_BOS_HEADER_DESCRIPTOR_INIT(MSOS20_BOS_TOTAL_LEN, MSOS20_BOS_NUM_CAPS),              \
        USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_INIT(MSOS_VENDOR_CODE, MSOS20_SET_LEN_XBOX)      \
        , WEBUSB_DESC














/** **************************************************** Report Descriptor **************************************************** */
#define DESC_RPT_KBD                                                        \
    /* ---- Report ID 1: NKRO Keyboard ---- */                              \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP                           ),    \
    HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD                       ),    \
    HID_COLLECTION ( HID_COLLECTION_APPLICATION                       ),    \
        HID_REPORT_ID  ( REPORT_ID_NKRO                               )     \
        /* modifier (usages 224..231) */                                    \
        HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD                      ),    \
            HID_USAGE_MIN    ( 224                                    ),    \
            HID_USAGE_MAX    ( 231                                    ),    \
            HID_LOGICAL_MIN  ( 0                                      ),    \
            HID_LOGICAL_MAX  ( 1                                      ),    \
            HID_REPORT_SIZE  ( 1                                      ),    \
            HID_REPORT_COUNT ( 8                                      ),    \
            HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),    \
            /* reserved */                                                  \
            HID_REPORT_SIZE  ( 8                                      ),    \
            HID_REPORT_COUNT ( 1                                      ),    \
            HID_INPUT        ( HID_CONSTANT                           ),    \
        /* LED output (NumLock/CapsLock/ScrollLock/Compose/Kana) + 3 pad */ \
        HID_USAGE_PAGE  ( HID_USAGE_PAGE_LED                          ),    \
            HID_USAGE_MIN    ( 1                                      ),    \
            HID_USAGE_MAX    ( 5                                      ),    \
            HID_REPORT_SIZE  ( 1                                      ),    \
            HID_REPORT_COUNT ( 5                                      ),    \
            HID_OUTPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),    \
            HID_REPORT_SIZE  ( 3                                      ),    \
            HID_REPORT_COUNT ( 1                                      ),    \
            HID_OUTPUT       ( HID_CONSTANT                           ),    \
        /* NKRO 位图: keycode 0..127 => 128 bit = 16B */                    \
        HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD                      ),    \
            HID_USAGE_MIN    ( 0                                      ),    \
            HID_USAGE_MAX    ( 127                                    ),    \
            HID_LOGICAL_MIN  ( 0                                      ),    \
            HID_LOGICAL_MAX  ( 1                                      ),    \
            HID_REPORT_SIZE  ( 1                                      ),    \
            HID_REPORT_COUNT ( 128                                    ),    \
            HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),    \
    HID_COLLECTION_END,                                                     \
                                                                            \
    /* ---- Report ID 2: Consumer Control ---- */                           \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_CONSUMER                   ),           \
    HID_USAGE      ( HID_USAGE_CONSUMER_CONTROL                ),           \
    HID_COLLECTION ( HID_COLLECTION_APPLICATION                ),           \
        HID_REPORT_ID  ( REPORT_ID_CONSUMER                    )            \
        HID_USAGE_MIN    ( 0                                   ),           \
        HID_USAGE_MAX_N  ( 0x03FF, 2                           ),           \
        HID_LOGICAL_MIN  ( 0x00                                ),           \
        HID_LOGICAL_MAX_N( 0x03FF, 2                           ),           \
        HID_REPORT_SIZE  ( 16                                  ),           \
        HID_REPORT_COUNT ( 1                                   ),           \
        HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ),           \
    HID_COLLECTION_END,                                                     \
                                                                            \
    /* ---- Report ID 3: System Control ---- */                             \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP                    ),           \
    HID_USAGE      ( HID_USAGE_DESKTOP_SYSTEM_CONTROL          ),           \
    HID_COLLECTION ( HID_COLLECTION_APPLICATION                ),           \
        HID_REPORT_ID  ( REPORT_ID_SYSTEM                      )            \
        HID_USAGE        ( HID_USAGE_DESKTOP_SYSTEM_POWER_DOWN ),           \
        HID_USAGE        ( HID_USAGE_DESKTOP_SYSTEM_SLEEP      ),           \
        HID_USAGE        ( HID_USAGE_DESKTOP_SYSTEM_WAKE_UP    ),           \
        HID_LOGICAL_MIN  ( 0                                   ),           \
        HID_LOGICAL_MAX  ( 3                                   ),           \
        HID_REPORT_SIZE  ( 8                                   ),           \
        HID_REPORT_COUNT ( 1                                   ),           \
        HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ),           \
    HID_COLLECTION_END,                                                     \
                                                                            \
    /* ---- Report ID 4: 6KRO Keyboard ---- */                              \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP                           ),    \
    HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD                       ),    \
    HID_COLLECTION ( HID_COLLECTION_APPLICATION                       ),    \
        HID_REPORT_ID  ( REPORT_ID_6KRO                               )     \
        /* modifier */                                                      \
        HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD                      ),    \
            HID_USAGE_MIN    ( 224                                    ),    \
            HID_USAGE_MAX    ( 231                                    ),    \
            HID_LOGICAL_MIN  ( 0                                      ),    \
            HID_LOGICAL_MAX  ( 1                                      ),    \
            HID_REPORT_SIZE  ( 1                                      ),    \
            HID_REPORT_COUNT ( 8                                      ),    \
            HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),    \
            /* reserved */                                                  \
            HID_REPORT_SIZE  ( 8                                      ),    \
            HID_REPORT_COUNT ( 1                                      ),    \
            HID_INPUT        ( HID_CONSTANT                           ),    \
        /* 6 keycodes (array, 0..101) */                                    \
        HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD                      ),    \
            HID_USAGE_MIN    ( 0                                      ),    \
            HID_USAGE_MAX    ( 101                                    ),    \
            HID_LOGICAL_MIN  ( 0                                      ),    \
            HID_LOGICAL_MAX  ( 101                                    ),    \
            HID_REPORT_SIZE  ( 8                                      ),    \
            HID_REPORT_COUNT ( 6                                      ),    \
            HID_INPUT        ( HID_DATA | HID_ARRAY                   ),    \
    HID_COLLECTION_END,                                                     \
                                                                            \
    /* ---- Report ID 5: Mouse ---- */                                      \
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE))

#define DESC_RPT_KBD_SIZ ((uint16_t)(sizeof((const char[]){ DESC_RPT_KBD })))


#if !HAL_USB_CC_WINUSB_ENABLE
    #define DESC_RPT_CC TUD_HID_REPORT_DESC_GENERIC_INOUT(INTF_CC_EP_SIZE)
    #define DESC_RPT_CC_SIZ ((uint16_t)(sizeof((const char[]){ DESC_RPT_CC })))
#endif


#define DESC_RPT_LAMP TUD_HID_REPORT_DESC_LIGHTING(LIGHTING_REPORT_ID_ATTRIBUTES)
#define DESC_RPT_LAMP_SIZ ((uint16_t)(sizeof((const char[]){ DESC_RPT_LAMP })))


#if !HAL_USB_DT_WINUSB_ENABLE
    #define DESC_RPT_DT TUD_HID_REPORT_DESC_GENERIC_INOUT(INTF_DT_EP_SIZE)
    #define DESC_RPT_DT_SIZ ((uint16_t)(sizeof((const char[]){ DESC_RPT_DT })))
#endif


/* ---- Gamepad 报告描述符：独立接口，无 Report ID，定长 13 字节 ----
 * byte0-1  : 16 个按键 (bit0..15)
 * byte2    : 方向帽 (低 4 位，0..7；高 4 位为描述符里的常量 padding)
 * byte3-10 : 左摇杆 X/Y、右摇杆 Z/Rx (int16 LE，-32767..32767)
 * byte11-12: 左/右扳机 Ry/Rz (uint8，0..255)
 * 位布局与 hal_gamepad.c 的编码函数严格对应，改动时两边同步。
 * 注意扳机的逻辑范围必须用 2 字节版本：HID_LOGICAL_MAX(255) 是 1 字节有符号，
 * 0xFF 会被解析成 -1，导致 min(0) > max(-1)，Windows 的 joy.sys 会拒绝该设备。 */
#define DESC_RPT_GAMEPAD                                                       \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP                          ),        \
    HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD                       ),        \
    HID_COLLECTION ( HID_COLLECTION_APPLICATION                      ),        \
        /* 16 buttons */                                                       \
        HID_USAGE_PAGE  ( HID_USAGE_PAGE_BUTTON                      ),        \
        HID_USAGE_MIN   ( 1                                          ),        \
        HID_USAGE_MAX   ( 16                                         ),        \
        HID_LOGICAL_MIN ( 0                                          ),        \
        HID_LOGICAL_MAX ( 1                                          ),        \
        HID_REPORT_SIZE ( 1                                          ),        \
        HID_REPORT_COUNT( 16                                         ),        \
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE     ),        \
        /* hat switch：4 位，0..7 = 8 个方向，超出范围表示居中 */                 \
        HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP                     ),        \
        HID_USAGE       ( HID_USAGE_DESKTOP_HAT_SWITCH               ),        \
        HID_LOGICAL_MIN ( 0                                          ),        \
        HID_LOGICAL_MAX ( 7                                          ),        \
        HID_PHYSICAL_MIN( 0                                          ),        \
        HID_PHYSICAL_MAX_N ( 315, 2                                  ),        \
        HID_REPORT_SIZE ( 4                                          ),        \
        HID_REPORT_COUNT( 1                                          ),        \
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE     ),        \
        /* 4 位 padding，与 hat 凑满 1 字节 */                                  \
        HID_REPORT_SIZE ( 4                                          ),        \
        HID_REPORT_COUNT( 1                                          ),        \
        HID_INPUT       ( HID_CONSTANT                               ),        \
        /* 左摇杆 X/Y (int16) */                                               \
        HID_USAGE       ( HID_USAGE_DESKTOP_X                        ),        \
        HID_USAGE       ( HID_USAGE_DESKTOP_Y                        ),        \
        HID_LOGICAL_MIN_N ( -32767, 2                                ),        \
        HID_LOGICAL_MAX_N ( 32767, 2                                 ),        \
        HID_REPORT_SIZE ( 16                                         ),        \
        HID_REPORT_COUNT( 2                                          ),        \
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE     ),        \
        /* 右摇杆：Z + Rx (int16) */                                            \
        HID_USAGE       ( HID_USAGE_DESKTOP_Z                        ),        \
        HID_USAGE       ( HID_USAGE_DESKTOP_RX                       ),        \
        HID_LOGICAL_MIN_N ( -32767, 2                                ),        \
        HID_LOGICAL_MAX_N ( 32767, 2                                 ),        \
        HID_REPORT_SIZE ( 16                                         ),        \
        HID_REPORT_COUNT( 2                                          ),        \
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE     ),        \
        /* 扳机 Ry/Rz (uint8) */                                               \
        HID_USAGE       ( HID_USAGE_DESKTOP_RY                       ),        \
        HID_USAGE       ( HID_USAGE_DESKTOP_RZ                       ),        \
        HID_LOGICAL_MIN_N ( 0, 2                                     ),        \
        HID_LOGICAL_MAX_N ( 255, 2                                   ),        \
        HID_REPORT_SIZE ( 8                                          ),        \
        HID_REPORT_COUNT( 2                                          ),        \
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE     ),        \
    HID_COLLECTION_END

#define DESC_RPT_GAMEPAD_SIZ ((uint16_t)(sizeof((const char[]){ DESC_RPT_GAMEPAD })))


#endif
