#ifndef YMODEM_H
#define YMODEM_H

#include <stdint.h>
#include "rotsta.h"

typedef rotsta_t (*ymodem_xfer_cb_t)(const uint8_t *buf, uint32_t buf_siz);



#endif

