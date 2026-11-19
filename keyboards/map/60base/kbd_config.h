#ifndef KBD_CONFIG_H
#define KBD_CONFIG_H

#include <stdint.h>

#define KBD_MAP_MAX         (2)     /* 层数 */
#define KBD_KEY_MAX         (42)    /* 按键数 */

#ifndef KBD_CK_KI
#define KBD_CK_KI(ki) ((ki) < KBD_KEY_MAX)
#endif

#ifndef KBD_CK_KM
#define KBD_CK_KM(km) ((km) < KBD_MAP_MAX)
#endif

#define KBD_KEY_DT          ( 1 )   /* unit: um */
#define KBD_KEY_UM2MM(um)   ( (float)(um) * 0.001f )
#define KBD_KEY_MM2UM(mm)   ( (float)(mm) * 1000.0f )

extern const uint32_t keymap[KBD_MAP_MAX][KBD_KEY_MAX];

#endif  /* KBD_CONFIG_H */
