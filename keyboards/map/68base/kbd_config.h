#ifndef KBD_CONFIG_H
#define KBD_CONFIG_H

#include <stdint.h>

#define KBD_SYS_MAX         (3)     /* 系统数 */
#define KBD_MAP_MAX         (4)     /* 层数 */
#define KBD_KEY_MAX         (68)    /* 按键数 */

#ifndef KBD_CK_KI
#define KBD_CK_KI(ki) ((ki) < KBD_KEY_MAX)
#endif

#ifndef KBD_CK_KM
#define KBD_CK_KM(km) ((km) < KBD_MAP_MAX)
#endif


#define KBD_KEY_DU              (1)
#define KBD_KEY_UM2MM(um)       ((float)(um) * 0.001f)
#define KBD_KEY_MM2UM(mm)       ((float)(mm) * 1000.0f)
#define KBD_ADC_BIT             (12)
#define KBD_ADC_VEF             (3300)
#define KBD_PCB_THICKNESS       (1600)
#define KBD_PCB_TO_HALL_DIST    (510)

#define KBD_KEY_DT              (125)



#endif  /* KBD_CONFIG_H */
