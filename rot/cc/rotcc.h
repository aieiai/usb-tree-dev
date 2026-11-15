// Rays Of Trace Control Center
#ifndef ROTCC_H
#define ROTCC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rotgen_proto.h"
#include "rotsta.h"

#ifndef ROTCC_RX_LOG_ENABLE
#define ROTCC_RX_LOG_ENABLE 1
#endif


enum {
    rotsta_rotcc_init_fail = ROTSTA_MAKE(rotsta_group_rotcc, 1),
    rotsta_rotcc_not_init,
    rotsta_rotcc_tx_timeout,
    rotsta_rotcc_tx_lock_fail,
};

/*
 * Receive path: the USB OUT callback only appends bytes to an internal ring
 * buffer and wakes the CC thread; the thread feeds the bytes into the Rot-Gen
 * layer, which reassembles frames and calls the registered frame callback.
 *
 * Send path: no queue. The frame is handed to the USB endpoint immediately and
 * the caller buffer is free again when rotcc_send_frame() returns.
 */
rotsta_t rotcc_init(void);

/* Rot-Gen frame handler, invoked from the CC thread. Pass NULL to detach. */
void rotcc_register_frame_cb(rotgen_frame_fn_t cb);

/* Sends one payload to the Rot control center. Thread context only. */
rotsta_t rotcc_send_frame(uint8_t options, const void *payload, uint16_t payload_len);

/* True while a frame is still on its way to the host. */
bool rotcc_is_busy(void);
bool rotcc_wait_idle(uint32_t timeout_ms);
uint32_t rotcc_dropped(void);
uint32_t rotcc_tx_dropped(void);


#ifdef __cplusplus
}
#endif

#endif
