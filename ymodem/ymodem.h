#ifndef YMODEM_H
#define YMODEM_H

#include <stdint.h>
#include "rot_state.h"

typedef rot_state_t (*ymodem_xfer_cb_t)(const uint8_t *buf, uint32_t buf_siz);



#endif

