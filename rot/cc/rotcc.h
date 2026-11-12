// Rays Of Trace Control Center
#ifndef ROTCC_H
#define ROTCC_H

#include <stdbool.h>
#include <stdint.h>

#include "rotsta.h"

#ifndef ROTCC_RX_LOG_ENABLE
#define ROTCC_RX_LOG_ENABLE 1
#endif

enum {
    rotsta_rotcc_init_fail = ROTSTA_MAKE(rotsta_group_rotcc, 1),
    rotsta_rotcc_not_init,
    rotsta_rotcc_tx_full,
    rotsta_rotcc_tx_lock_fail,
};

rotsta_t rotcc_init(void);

/*
 * Queue one packet for transmission. Success means the packet was queued;
 * the transfer result is reported by rot_cc_on_tx_complete(). Thread context
 * only, because queueing may wait briefly for the TX index mutex.
 */
rotsta_t rotcc_send(const void *data, uint32_t len);
bool rotcc_is_busy(void);
bool rotcc_wait_idle(uint32_t timeout_ms);
uint32_t rotcc_dropped(void);
uint32_t rotcc_tx_dropped(void);


#endif
