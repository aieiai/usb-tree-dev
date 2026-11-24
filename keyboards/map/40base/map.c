
#include <stddef.h>
#include "keycode.h"
#include "kbd_config.h"

/** 40% keyboard layout
 * ,------------------------------------------------------------------------------------.
 * | Esc  |   Q  |   W  |   E  |   R  |   T  |   Y  |   U  |   I  |   O  |   P  | Space |
 * |------+------+------+------+------+------+------+------+------+------+------+-------|
 * | Tab  |   A  |   S  |   D  |   F  |   G  |   H  |   J  |   K  |   L  |     Enter    |
 * |------+------+------+------+------+------+------+------+------+------+--------------|
 * | Shift|   Z  |   X  |   C  |   V  |   B  |   N  |   M  |   ,  |  Del |     Shift    |
 * |------+------+------+------+------+------+------+------+------+------+------+-------|
 * | Ctrl | GUI  | Alt  |    Space    |        Fn          | Ctrl |  GUI |     Alt      |
 * `------------------------------------------------------------------------------------'
 */
/** @brief  keyboard mapping layout 
 *  @note
 *  1. Windows keyboard layout is the same as Linux keyboard layout.
*/

static const uint32_t keymap[KBD_SYS_MAX][KBD_MAP_MAX][KBD_KEY_MAX] = {
    [0] = {
        {
            //0             1               2               3               4               5               6               7           8               9               10          11
            _K(KC_ESC),     _K(KC_Q),       _K(KC_W),       _K(KC_E),       _K(KC_R),       _K(KC_T),       _K(KC_Y),       _K(KC_U),   _K(KC_I),       _K(KC_O),       _K(KC_P),   _K(KC_BACKSPACE),
            //12            13              14              15              16              17              18              19          20              21              22
            _K(KC_TAB),     _K(KC_A),       _K(KC_S),       _K(KC_D),       _K(KC_F),       _K(KC_G),       _K(KC_H),       _K(KC_J),   _K(KC_K),       _K(KC_L),       _K(KC_ENTER),
            //23            24              25              26              27              28              29              30          31              32              33
            _K(KC_LSFT),    _K(KC_Z),       _K(KC_X),       _K(KC_C),       _K(KC_V),       _K(KC_B),       _K(KC_N),       _K(KC_M),   _K(KC_COMMA),   _K(KC_DELETE),  _K(KC_RSFT),
            //34            35              36              37                              38                                          39              40              41
            _K(KC_LCTL),    _K(KC_LGUI),    _K(KC_LALT),    _K(KC_SPACE),                   _FN(KC_FN1),                                _K(KC_RCTL),    _K(KC_RGUI),    _K(KC_RALT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
    },
    [1] = {
        {
            //0             1               2               3               4               5               6               7           8               9               10          11
            _K(KC_ESC),     _K(KC_Q),       _K(KC_W),       _K(KC_E),       _K(KC_R),       _K(KC_T),       _K(KC_Y),       _K(KC_U),   _K(KC_I),       _K(KC_O),       _K(KC_P),   _K(KC_BACKSPACE),
            //12            13              14              15              16              17              18              19          20              21              22
            _K(KC_TAB),     _K(KC_A),       _K(KC_S),       _K(KC_D),       _K(KC_F),       _K(KC_G),       _K(KC_H),       _K(KC_J),   _K(KC_K),       _K(KC_L),       _K(KC_ENTER),
            //23            24              25              26              27              28              29              30          31              32              33
            _K(KC_LSFT),    _K(KC_Z),       _K(KC_X),       _K(KC_C),       _K(KC_V),       _K(KC_B),       _K(KC_N),       _K(KC_M),   _K(KC_COMMA),   _K(KC_DELETE),  _K(KC_RSFT),
            //34            35              36              37                              38                                          39              40              41
            _K(KC_LCTL),    _K(KC_LGUI),    _K(KC_LALT),    _K(KC_SPACE),                   _FN(KC_FN1),                                _K(KC_RCTL),    _K(KC_RGUI),    _K(KC_RALT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
    },
    [2] = {
        {
            //0             1               2               3               4               5               6               7           8               9               10          11
            _K(KC_ESC),     _K(KC_Q),       _K(KC_W),       _K(KC_E),       _K(KC_R),       _K(KC_T),       _K(KC_Y),       _K(KC_U),   _K(KC_I),       _K(KC_O),       _K(KC_P),   _K(KC_BACKSPACE),
            //12            13              14              15              16              17              18              19          20              21              22
            _K(KC_TAB),     _K(KC_A),       _K(KC_S),       _K(KC_D),       _K(KC_F),       _K(KC_G),       _K(KC_H),       _K(KC_J),   _K(KC_K),       _K(KC_L),       _K(KC_ENTER),
            //23            24              25              26              27              28              29              30          31              32              33
            _K(KC_LSFT),    _K(KC_Z),       _K(KC_X),       _K(KC_C),       _K(KC_V),       _K(KC_B),       _K(KC_N),       _K(KC_M),   _K(KC_COMMA),   _K(KC_DELETE),  _K(KC_RSFT),
            //34            35              36              37                              38                                          39              40              41
            _K(KC_LCTL),    _K(KC_LGUI),    _K(KC_LALT),    _K(KC_SPACE),                   _FN(KC_FN1),                                _K(KC_RCTL),    _K(KC_RGUI),    _K(KC_RALT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
        {
            //0         1           2           3           4           5           6           7           8               9               10          11
            _K(KC_F1),  _K(KC_F2),  _K(KC_F3),  _K(KC_F4),  _K(KC_F5),  _K(KC_F6),  _K(KC_F7),  _K(KC_F8),  _K(KC_F9),      _K(KC_F10),     _K(KC_F11), _K(KC_F12),
            //12        13          14          15          16          17          18          19          20              21              22
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            ___,            ___,
            //23        24          25          26          27          28          29          30          31              32              33
            ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,        ___,            _K(KC_UP),      ___,
            //34        35          36          37                      38                                  39              40              41
            ___,        ___,        ___,        ___,                    ___,                                _K(KC_LEFT),    _K(KC_DOWN),    _K(KC_RIGHT)
        },
    },
};

#include "drv_hall.h"
#ifndef SENSOR_TOTAL_SIZE
#define SENSOR_TOTAL_SIZE 72
#endif

static const uint8_t ks2ki[SENSOR_TOTAL_SIZE] = {
    22,  24,  28,  25,  29,  27,  15,  16,  19,  17,  21,  18,  
    23,  39,  56,  62,  67,  64,  59,  60,  35,  255, 61,  4,   
    8,   10,  13,  11,  14,  12,  0,   1,   5,   2,   6,   3,   
    9,   52,  65,  26,  66,  63,  58,  255, 20,  255, 7,   255, 
    50,  51,  55,  53,  57,  54,  44,  45,  48,  46,  49,  47,  
    37,  38,  42,  40,  43,  41,  30,  31,  34,  32,  36,  33,  
};

const uint32_t *keymap_get_default(uint8_t sys, uint8_t map)
{
    if ((sys >= KBD_SYS_MAX) || (map >= KBD_MAP_MAX)) {
        return NULL;
    }
    return keymap[sys][map];
}

uint8_t keymap_get_ki(uint8_t si)
{
    if (si >= SENSOR_TOTAL_SIZE) {
        return 0xff;
    }
    return ks2ki[si];
}
