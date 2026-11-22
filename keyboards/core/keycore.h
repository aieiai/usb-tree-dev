#ifndef KEY_CORE_H
#define KEY_CORE_H

#include "rotsta.h"
#include "kbd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DT
#   ifdef KBD_DT
#       define DT   (KBD_DT)
#   else
#       define DT   (125)
#   endif
#endif


rotsta_t keycore_init(void);

#ifdef __cplusplus
}
#endif

#endif  /* KEY_CORE_H */
