#ifndef ROT_CC_H
#define ROT_CC_H

#include <stdbool.h>
#include <stdint.h>
#include "rot_state.h"


void rot_cc_init(void);
int rot_cc_send(const void *data, uint32_t len);
bool rot_cc_is_busy(void);
uint32_t rot_cc_dropped(void);


#endif
