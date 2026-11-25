#ifndef KEY_CORE_H
#define KEY_CORE_H

#include "rotsta.h"
#include "kbd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// unit: us
#ifndef DT
#   ifdef KBD_DT
#       define DT       (KBD_DT)
#   else
#       define DT       (125)
#   endif
#endif

#if (DT == 125) && !defined(DT_SHIFT)
#   define DT_SHIFT     (3)         /* 1000 / 125 = 8 = 2^3 */
#endif

#ifndef US2TK
#   ifdef KBD_US2TK
#       define US2TK(us)    (KBD_US2TK(us))
#   else
#       define US2TK(us)    (((uint32_t)(us)) / (DT))
#   endif
#endif

#ifndef TK2US
#   ifdef KBD_TK2US
#       define TK2US(tk)    (KBD_TK2US(tk))
#   else
#       define TK2US(tk)    (((uint32_t)(tk)) * (DT))
#   endif
#endif

#ifndef MS2TK
#   ifdef KBD_MS2TK
#       define MS2TK(ms)    (KBD_MS2TK(ms))
#   elif defined(DT_SHIFT)
#       define MS2TK(ms)    (((uint32_t)(ms)) << (DT_SHIFT))
#   else
#       define MS2TK(ms)    ((((uint32_t)(ms)) * 1000u) / (DT))
#   endif
#endif

#ifndef TK2MS
#   ifdef KBD_TK2MS
#       define TK2MS(tk)    (KBD_TK2MS(tk))
#   elif defined(DT_SHIFT)
#       define TK2MS(tk)    (((uint32_t)(tk)) >> (DT_SHIFT))
#   else
#       define TK2MS(tk)    ((((uint32_t)(tk)) * (DT)) / 1000u)
#   endif
#endif


extern uint32_t core_freq_mhz;
rotsta_t keycore_init(void);

#ifdef __cplusplus
}
#endif

#endif  /* KEY_CORE_H */
