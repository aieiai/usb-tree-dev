#ifndef ROTCC_UPGRADE_H
#define ROTCC_UPGRADE_H

#include <stdint.h>

#include "rotcc_protocol.h"

/* USB-facing adapter: validates frames and queues work to the upgrade task. */
void rotcc_upgrade_on_rx(const uint8_t *data, uint16_t len);


#endif
