#include "rotcc.h"

#include <stddef.h>
#include <string.h>

#include "hal_usb.h"
#include "rotrb.h"

#include "tx_api.h"
#include "tx_config.h"


#define TAG "rotcc: "

/* Receive ring: byte stream fed by the USB OUT callback and drained by the CC
 * thread. Must be a power of two and hold at least two full-size packets. */
#define ROTCC_RX_BUF_SIZE       (INTF_CC_EP_SIZE * 2U)
#define ROTCC_RX_CHUNK_SIZE     (64U)
#define ROTCC_TX_TIMEOUT_MS     (1000U)
#define ROTCC_TX_TIMEOUT_TICKS  ((ROTCC_TX_TIMEOUT_MS * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U)


enum {
    ROTCC_EVT_RX         = (1UL << 0),
    ROTCC_EVT_LINK_READY = (1UL << 1),
};

enum {
    ROTCC_EVT_TX_DONE    = (1UL << 0),
};

typedef struct {
    rotrb_t rx_rb;
    uint8_t rx_buf[ROTCC_RX_BUF_SIZE] __attribute__((aligned(4)));
    uint8_t frame_buf[INTF_CC_EP_SIZE] __attribute__((aligned(4)));
    rotgen_t proto;

    volatile uint32_t rx_dropped;
    volatile uint32_t tx_dropped;
    bool initialized;
} rotcc_msg_t;


static TX_EVENT_FLAGS_GROUP rx_events;
static TX_EVENT_FLAGS_GROUP tx_events;
static TX_MUTEX tx_mutex;
static TX_THREAD thread_rotcc;
static uint8_t thread_rotcc_stack[ROTCC_TASK_STACK] __attribute__((aligned(8U)));

static rotcc_msg_t cc_msg;
static rotgen_frame_fn_t frame_cb = NULL;

_Static_assert((ROTCC_RX_BUF_SIZE & (ROTCC_RX_BUF_SIZE - 1U)) == 0U, "RX ring size must be a power of two");
_Static_assert(ROTCC_RX_BUF_SIZE >= (2U * INTF_CC_EP_SIZE), "RX ring must hold at least two CC packets");
_Static_assert(sizeof(cc_msg.frame_buf) <= ROTGEN_MAX_FRAME_SIZE, "Rot-Gen frame buffer limit is too small");
_Static_assert(INTF_CC_EP_SIZE <= ROTGEN_MAX_FRAME_SIZE, "CC endpoint is larger than one Rot-Gen frame");

static void _rx_callback(const uint8_t *data, uint16_t len, void *ctx)
{
    uint32_t written;

    (void)ctx;
    if ((data == NULL) || (len == 0U) || (len > INTF_CC_EP_SIZE)) {
        return;
    }

    written = rotrb_in(&cc_msg.rx_rb, (void *)data, len);
    if (written < len) {
        cc_msg.rx_dropped += (uint32_t)len - written;
    }

    (void)tx_event_flags_set(&rx_events, ROTCC_EVT_RX, TX_OR);
}

static void _configured_callback(void *ctx)
{
    (void)ctx;
    (void)tx_event_flags_set(&rx_events, ROTCC_EVT_LINK_READY, TX_OR);
}

/* Runs in USB interrupt context, only signals the sender. */
static void _tx_complete_callback(rotsta_t state, uint16_t len, void *ctx)
{
    (void)state;
    (void)len;
    (void)ctx;
    (void)tx_event_flags_set(&tx_events, ROTCC_EVT_TX_DONE, TX_OR);
}

static void _frame_callback(const rotgen_frame_t *frame, void *ctx)
{
    (void)ctx;
    if (frame_cb != NULL) {
        frame_cb(frame, NULL);
    }
}

static void _tx_clear_done(void)
{
    ULONG events = 0UL;
    (void)tx_event_flags_get(&tx_events, ROTCC_EVT_TX_DONE, TX_OR_CLEAR, &events, TX_NO_WAIT);
}

static bool _tx_wait_done(void)
{
    ULONG events = 0UL;
    return tx_event_flags_get(&tx_events, ROTCC_EVT_TX_DONE, TX_OR_CLEAR, &events, ROTCC_TX_TIMEOUT_TICKS) == TX_SUCCESS;
}

static rotsta_t _tx_start(const void *data, uint32_t len)
{
    ULONG start = tx_time_get();

    for (;;) {
        rotsta_t state = (rotsta_t)hal_cc_send(data, len);

        if (state != rotsta_hal_cc_busy) {
            return state;
        }
        if ((ULONG)(tx_time_get() - start) >= ROTCC_TX_TIMEOUT_TICKS) {
            return rotsta_rotcc_tx_timeout;
        }
        tx_thread_sleep(1);
    }
}

/* Raw endpoint send, used as the Rot-Gen transport primitive. */
static rotsta_t rotcc_send(const uint8_t *data, uint16_t len, void *ctx)
{
    rotsta_t state;

    (void)ctx;

    if (!cc_msg.initialized) {
        return rotsta_rotcc_not_init;
    }
    if ((data == NULL) || (len == 0U) || (len > INTF_CC_EP_SIZE)) {
        return rotsta_invalid_argument;
    }
    if (tx_mutex_get(&tx_mutex, TX_WAIT_FOREVER) != TX_SUCCESS) {
        return rotsta_rotcc_tx_lock_fail;
    }

    _tx_clear_done();
    state = _tx_start(data, len);
    if ((state == rotsta_success) && !_tx_wait_done()) {
        state = rotsta_rotcc_tx_timeout;
    }

    (void)tx_mutex_put(&tx_mutex);

    if (state != rotsta_success) {
        cc_msg.tx_dropped += len;
    }
    return state;
}

static void rotcc_task(ULONG id)
{
    uint8_t chunk[ROTCC_RX_CHUNK_SIZE];

    (void)id;

    while (1) {
        ULONG events = 0U;
        if (tx_event_flags_get(&rx_events, (ROTCC_EVT_RX | ROTCC_EVT_LINK_READY), TX_OR_CLEAR, &events, TX_WAIT_FOREVER) != TX_SUCCESS) {
            continue;
        }

        for (;;) {
            uint32_t len = rotrb_peek(&cc_msg.rx_rb, chunk, sizeof(chunk));

            if (len == 0U) {
                break;
            }
            rotgen_feed(&cc_msg.proto, chunk, (uint16_t)len);
            (void)rotrb_drop(&cc_msg.rx_rb, len);
        }

        if (cc_msg.proto.dropped != 0U) {
            uint32_t dropped = cc_msg.proto.dropped;

            cc_msg.proto.dropped = 0U;
            cc_msg.rx_dropped += dropped;
#if ROTCC_RX_LOG_ENABLE
            hal_dt_printf(TAG "resync dropped %u byte(s)\r\n", (unsigned)dropped);
#endif
        }
    }
}

rotsta_t rotcc_init(void)
{
    UINT ret;

    if (cc_msg.initialized) {
        return rotsta_success;
    }

    cc_msg.rx_dropped = 0U;
    cc_msg.tx_dropped = 0U;
    (void)rotrb_init(&cc_msg.rx_rb, cc_msg.rx_buf, sizeof(cc_msg.rx_buf));
    if (!rotgen_init(&cc_msg.proto, cc_msg.frame_buf, sizeof(cc_msg.frame_buf), ROTGEN_ADDR_KEYBOARD, rotcc_send, _frame_callback, NULL)) {
        printf(TAG "protocol init failed\r\n");
        return rotsta_fail;
    }

    if (tx_event_flags_create(&rx_events, "rotcc_rx_events") != TX_SUCCESS) {
        printf(TAG "event flags create failed\r\n");
        return rotsta_fail;
    }
    if (tx_event_flags_create(&tx_events, "rotcc_tx_events") != TX_SUCCESS) {
        printf(TAG "tx event flags create failed\r\n");
        return rotsta_fail;
    }
    if (tx_mutex_create(&tx_mutex, "rotcc_tx_lock", TX_INHERIT) != TX_SUCCESS) {
        printf(TAG "tx mutex create failed\r\n");
        return rotsta_fail;
    }
    if (hal_cc_register_callbacks(_rx_callback, _tx_complete_callback, _configured_callback, NULL) != rotsta_success) {
        printf(TAG "callback registration failed\r\n");
        return rotsta_fail;
    }

    ret = tx_thread_create(&thread_rotcc, "thread_rot_cc", rotcc_task, 0U, thread_rotcc_stack, sizeof(thread_rotcc_stack), ROTCC_TASK_PRIORITY, ROTCC_TASK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ret != TX_SUCCESS) {
        (void)hal_cc_register_callbacks(NULL, NULL, NULL, NULL);
        printf(TAG "thread create failed, ret=%u\r\n", (unsigned)ret);
        return rotsta_fail;
    }
    cc_msg.initialized = true;
    return rotsta_success;
}

void rotcc_register_frame_cb(rotgen_frame_fn_t cb)
{
    frame_cb = cb;
}

rotsta_t rotcc_send_frame(uint8_t options, const void *payload, uint16_t payload_len)
{
    if (!cc_msg.initialized) {
        return rotsta_rotcc_not_init;
    }
    return rotgen_send(&cc_msg.proto, options, ROTGEN_ADDR_CC, payload, payload_len);
}

bool rotcc_is_busy(void)
{
    return hal_cc_is_busy();
}

bool rotcc_wait_idle(uint32_t timeout_ms)
{
    ULONG start = tx_time_get();
    ULONG timeout_ticks =
        (timeout_ms * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U;

    while (rotcc_is_busy()) {
        if ((ULONG)(tx_time_get() - start) >= timeout_ticks) {
            return false;
        }
        tx_thread_sleep(1);
    }
    return true;
}

uint32_t rotcc_dropped(void)
{
    return cc_msg.rx_dropped;
}

uint32_t rotcc_tx_dropped(void)
{
    return cc_msg.tx_dropped;
}
