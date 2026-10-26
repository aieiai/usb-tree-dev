#ifndef HID_REPORT_DESC_H
#define HID_REPORT_DESC_H

#define REPORT_DESC_KBD                                                     \
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


/* ---- Gamepad 报告描述符：独立接口，无 Report ID，定长 13 字节 ----
 * byte0-1  : 16 个按键 (bit0..15)
 * byte2    : 方向帽 (低 4 位，0..7；高 4 位为描述符里的常量 padding)
 * byte3-10 : 左摇杆 X/Y、右摇杆 Z/Rx (int16 LE，-32767..32767)
 * byte11-12: 左/右扳机 Ry/Rz (uint8，0..255)
 * 位布局与 hal_gamepad.c 的编码函数严格对应，改动时两边同步。
 * 注意扳机的逻辑范围必须用 2 字节版本：HID_LOGICAL_MAX(255) 是 1 字节有符号，
 * 0xFF 会被解析成 -1，导致 min(0) > max(-1)，Windows 的 joy.sys 会拒绝该设备。 */
#define REPORT_DESC_GAMEPAD                                                    \
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




#endif
