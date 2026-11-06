// Rays Of Trace Control Center
#ifndef ROTCC_H
#define ROTCC_H

#include <stdbool.h>
#include <stdint.h>

#include "rot_state.h"

#ifndef ROTCC_RX_LOG_ENABLE
#define ROTCC_RX_LOG_ENABLE 1
#endif

enum {
    rot_state_rotcc_init_fail = ROT_STATE_MAKE(rot_state_group_rotcc, 1),
    rot_state_rotcc_not_init,
    rot_state_rotcc_tx_full,
    rot_state_rotcc_tx_lock_fail,
};

rot_state_t rotcc_init(void);

/*
 * Queue one packet for transmission. Success means the packet was queued;
 * the transfer result is reported by rot_cc_on_tx_complete(). Thread context
 * only, because queueing may wait briefly for the TX index mutex.
 */
rot_state_t rotcc_send(const void *data, uint32_t len);
bool rotcc_is_busy(void);
uint32_t rotcc_dropped(void);
uint32_t rotcc_tx_dropped(void);


#endif
