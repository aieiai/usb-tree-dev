#ifndef HAL_CC_H
#define HAL_CC_H

#include <stdbool.h>
#include <stdint.h>
#include "rot_state.h"


enum {
    rot_state_hal_cc_fail = ROT_STATE_MAKE(rot_state_group_hal_cc, 1),
    rot_state_hal_cc_arg,
    rot_state_hal_cc_start,
    rot_state_hal_cc_busy,
    rot_state_hal_cc_not_ready,
};

typedef void (*hal_cc_rx_callback_t)(const uint8_t *data, uint16_t len, void *ctx);
typedef void (*hal_cc_tx_complete_callback_t)(rot_state_t state, uint16_t len, void *ctx);
typedef void (*hal_cc_configured_callback_t)(void *ctx);

typedef struct {
    hal_cc_rx_callback_t rx;
    hal_cc_tx_complete_callback_t tx_complete;
    void *ctx;
    hal_cc_configured_callback_t configured;
} hal_cc_callbacks_t;

/*
 * Register raw endpoint callbacks. These callbacks run in USB interrupt
 * context, so they must copy data and return quickly. Register them before the
 * host configures the interface; passing NULL clears all callbacks.
 */
int hal_cc_register_callbacks(const hal_cc_callbacks_t *callbacks);

/*
 * Queue one packet for transmission. Data is copied into the HAL TX buffer.
 * tx_complete is called in USB interrupt context after IN transfer completion.
 */
int hal_cc_send(const void *data, uint32_t len);

bool hal_cc_is_busy(void);

#endif
