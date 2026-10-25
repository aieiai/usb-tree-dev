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






#endif
