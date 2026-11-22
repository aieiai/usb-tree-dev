#ifndef KBD_CONFIG_H
#define KBD_CONFIG_H

#include <stdint.h>

#define KBD_SYS_MAX         (3)     /* 系统数 */
#define KBD_MAP_MAX         (4)     /* 层数 */
#define KBD_KEY_MAX         (42)    /* 按键数 */

#ifndef KBD_CK_KI
#define KBD_CK_KI(ki) ((ki) < KBD_KEY_MAX)
#endif

#ifndef KBD_CK_KM
#define KBD_CK_KM(km) ((km) < KBD_MAP_MAX)
#endif

#define KBD_ADC_BIT             (16)
#define KBD_ADC_VEF             (2500)
#define KBD_PCB_THICKNESS       (1600)
#define KBD_PCB_TO_HALL_DIST    (510)

#endif  /* KBD_CONFIG_H */
