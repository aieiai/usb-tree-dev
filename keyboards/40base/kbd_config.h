#ifndef KBD_CONFIG_H
#define KBD_CONFIG_H

#include <stdint.h>

#define KBD_MAP_MAX     (4)     /* 层数 */
#define KBD_KEY_MAX     (42)    /* 按键数 */

extern const uint32_t keymap[KBD_MAP_MAX][KBD_KEY_MAX];

#endif
