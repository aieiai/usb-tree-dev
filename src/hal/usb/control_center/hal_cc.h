#ifndef HAL_CC_H
#define HAL_CC_H

#include <stdbool.h>
#include <stdint.h>
#include "rotsta.h"


enum {
    rotsta_hal_cc_fail = ROTSTA_MAKE(rotsta_group_hal_cc, 1),
    rotsta_hal_cc_arg,
    rotsta_hal_cc_start,
    rotsta_hal_cc_busy,
    rotsta_hal_cc_not_ready,
};

typedef void (*hal_cc_rx_cb_t)(const uint8_t *data, uint16_t len, void *ctx);
typedef void (*hal_cc_tx_complete_cb_t)(rotsta_t state, uint16_t len, void *ctx);
typedef void (*hal_cc_config_cb_t)(void *ctx);


/*
 * Register raw endpoint callbacks. These callbacks run in USB interrupt
 * context, so they must copy data and return quickly. Register them before the
 * host configures the interface; passing NULL clears all callbacks.
 */
int hal_cc_register_callbacks(const hal_cc_rx_cb_t rx_cb, 
    const hal_cc_tx_complete_cb_t tx_complete_cb, 
    const hal_cc_config_cb_t configured_cb, 
    void *ctx);
/*
 * Start one transfer. The buffer is not copied, it must stay valid until
 * tx_complete fires. tx_complete is called in USB interrupt context after the
 * IN transfer completes.
 */
int hal_cc_send(const void *data, uint32_t len);

bool hal_cc_is_busy(void);

#endif
