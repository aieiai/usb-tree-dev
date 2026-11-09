#include <string.h>

#include "rotcc.h"
#include "rotcc_upgrade.h"
#include "hal_usb.h"

#include "tx_api.h"
#include "tx_config.h"


#define TAG "rotcc: "
#define ROTCC_PACKET_SIZE      (INTF_CC_EP_SIZE)
#define ROTCC_QUEUE_DEPTH      (2U)
#define ROTCC_QUEUE_MASK       (ROTCC_QUEUE_DEPTH - 1U)


enum {
    ROTCC_EVT_RX         = (1UL << 0),
    ROTCC_EVT_TX_SUBMIT  = (1UL << 1),
    ROTCC_EVT_TX_DONE    = (1UL << 2),
    ROTCC_EVT_LINK_READY = (1UL << 3),
};


typedef struct {
    uint16_t len;
    uint8_t data[ROTCC_PACKET_SIZE];
} __attribute__((aligned(4))) rotcc_packet_t;

enum {
    ROTCC_DIR_RX = 0,
    ROTCC_DIR_TX,
    ROTCC_DIR_MAX,
};

typedef struct {
    rotcc_packet_t packet_buf[ROTCC_DIR_MAX][ROTCC_QUEUE_DEPTH];
    uint32_t head[ROTCC_DIR_MAX];
    uint32_t tail[ROTCC_DIR_MAX];

    volatile uint32_t rx_dropped;
    volatile uint32_t tx_dropped;
    volatile rot_state_t tx_result;
    volatile uint16_t tx_len;
    bool initialized;
} rotcc_msg_t;


static TX_EVENT_FLAGS_GROUP cc_events;
static TX_MUTEX tx_enqueue_mutex;
static TX_THREAD thread_rotcc;
static uint8_t thread_rotcc_stack[ROTCC_TASK_STACK] __attribute__((aligned(8U)));

static rotcc_msg_t cc_msg;

_Static_assert((ROTCC_QUEUE_DEPTH & ROTCC_QUEUE_MASK) == 0U,
               "queue depth must be a power of two");

static void rotcc_rx_callback(const uint8_t *data, uint16_t len, void *ctx)
{
    rotcc_packet_t *packet;
    uint32_t head;
    uint32_t tail;

    (void)ctx;
    if ((data == NULL) || (len == 0U) || (len > ROTCC_PACKET_SIZE)) {
        return;
    }

    tail = __atomic_load_n(&cc_msg.tail[ROTCC_DIR_RX], __ATOMIC_RELAXED);
    head = __atomic_load_n(&cc_msg.head[ROTCC_DIR_RX], __ATOMIC_ACQUIRE);
    if ((uint32_t)(tail - head) >= ROTCC_QUEUE_DEPTH) {
        cc_msg.rx_dropped += len;
        return;
    }

    packet = &cc_msg.packet_buf[ROTCC_DIR_RX][tail & ROTCC_QUEUE_MASK];
    packet->len = len;
    memcpy(packet->data, data, len);

    __atomic_store_n(&cc_msg.tail[ROTCC_DIR_RX], tail + 1U, __ATOMIC_RELEASE);
    (void)tx_event_flags_set(&cc_events, ROTCC_EVT_RX, TX_OR);
}

static void rot_cc_tx_complete_callback(rot_state_t state, uint16_t len, void *ctx)
{
    (void)ctx;

    cc_msg.tx_result = state;
    cc_msg.tx_len = len;

    (void)tx_event_flags_set(&cc_events, ROTCC_EVT_TX_DONE, TX_OR);
}

static void rotcc_configured_callback(void *ctx)
{
    (void)ctx;
    (void)tx_event_flags_set(&cc_events, ROTCC_EVT_LINK_READY, TX_OR);
}

//更新rot_cc_on_rx_packet函数签名，将context参数改为ctx，并移除int result参数
__attribute__((weak)) void rot_cc_on_rx_packet(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)data;
    (void)len;
    (void)ctx;
}
__attribute__((weak)) void rot_cc_on_tx_complete(rot_state_t state, uint16_t len, void *ctx)
{
    (void)state;
    (void)len;
    (void)ctx;
}

static void rotcc_start_next_tx(void)
{
    for (;;) {
        rotcc_packet_t *packet;
        rot_state_t state;
        uint32_t head;
        uint32_t tail;
        uint16_t len;

        if (hal_cc_is_busy()) {
            return;
        }

        head = __atomic_load_n(&cc_msg.head[ROTCC_DIR_TX], __ATOMIC_RELAXED);
        tail = __atomic_load_n(&cc_msg.tail[ROTCC_DIR_TX], __ATOMIC_ACQUIRE);
        if (head == tail) {
            return;
        }

        packet = &cc_msg.packet_buf[ROTCC_DIR_TX][head & ROTCC_QUEUE_MASK];
        len = packet->len;
        state = (rot_state_t)hal_cc_send(packet->data, len);
        if ((state == rot_state_hal_cc_busy) || (state == rot_state_hal_cc_not_ready)) {
            return;
        }

        head++;
        __atomic_store_n(&cc_msg.head[ROTCC_DIR_TX], head, __ATOMIC_RELEASE);
        if (state == rot_state_success) {
            return;
        }

        cc_msg.tx_dropped += len;
        rot_cc_on_tx_complete(state, len, NULL);
    }
}

static void rotcc_drain_rx(void)
{
    uint32_t head = __atomic_load_n(&cc_msg.head[ROTCC_DIR_RX], __ATOMIC_RELAXED);

    for (;;) {
        rotcc_packet_t *packet;
        uint32_t tail = __atomic_load_n(&cc_msg.tail[ROTCC_DIR_RX], __ATOMIC_ACQUIRE);
        uint16_t len;

        if (head == tail) {
            return;
        }

        packet = &cc_msg.packet_buf[ROTCC_DIR_RX][head & ROTCC_QUEUE_MASK];
        len = packet->len;
#if ROTCC_RX_LOG_ENABLE
        if ((len < 2U) ||
            (packet->data[0] != ROTCC_FW_MAGIC_0) ||
            (packet->data[1] != ROTCC_FW_MAGIC_1)) {
            hal_dt_printf(TAG "host -> dev, %d bytes\r\n", len);
        }
#endif
        rot_cc_on_rx_packet(packet->data, len, NULL);

        head++;
        __atomic_store_n(&cc_msg.head[ROTCC_DIR_RX], head, __ATOMIC_RELEASE);
    }
}

static void rotcc_task(ULONG id)
{
    (void)id;

    while (1) {
        ULONG events = 0U;

        if (tx_event_flags_get(&cc_events, (ROTCC_EVT_RX | ROTCC_EVT_TX_SUBMIT | ROTCC_EVT_TX_DONE | ROTCC_EVT_LINK_READY), TX_OR_CLEAR, &events, TX_WAIT_FOREVER) == TX_SUCCESS) {
            if ((events & ROTCC_EVT_TX_DONE) != 0U) {
                rot_state_t tx_state = cc_msg.tx_result;
                uint16_t tx_len = cc_msg.tx_len;

                rot_cc_on_tx_complete(tx_state, tx_len, NULL);
            }

            rotcc_start_next_tx();
            rotcc_drain_rx();
        }
    }
}

rot_state_t rotcc_init(void)
{
    static const hal_cc_callbacks_t callbacks = {
        .rx = rotcc_rx_callback,
        .tx_complete = rot_cc_tx_complete_callback,
        .ctx = NULL,
        .configured = rotcc_configured_callback,
    };
    UINT ret;

    if (cc_msg.initialized) {
        return rot_state_success;
    }

    memset(cc_msg.head, 0, sizeof(cc_msg.head));
    memset(cc_msg.tail, 0, sizeof(cc_msg.tail));

    if (tx_event_flags_create(&cc_events, "rot_cc_events") != TX_SUCCESS) {
        printf(TAG "event flags create failed\r\n");
        return rot_state_fail;
    }
    if (tx_mutex_create(&tx_enqueue_mutex, "rotcc_tx_lock", TX_INHERIT) != TX_SUCCESS) {
        printf(TAG "tx mutex create failed\r\n");
        return rot_state_fail;
    }
    if (hal_cc_register_callbacks(&callbacks) != rot_state_success) {
        printf(TAG "callback registration failed\r\n");
        return rot_state_fail;
    }

    ret = tx_thread_create(&thread_rotcc, "thread_rot_cc", rotcc_task, 0U, thread_rotcc_stack, sizeof(thread_rotcc_stack), ROTCC_TASK_PRIORITY, ROTCC_TASK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ret != TX_SUCCESS) {
        (void)hal_cc_register_callbacks(NULL);
        printf(TAG "thread create failed, ret=%u\r\n", (unsigned)ret);
        return rot_state_fail;
    }
    cc_msg.initialized = true;
    return rot_state_success;
}

rot_state_t rotcc_send(const void *data, uint32_t len)
{
    rotcc_packet_t *packet;
    uint32_t head;
    uint32_t tail;

    if (!cc_msg.initialized) {
        return rot_state_rotcc_not_init;
    }
    if ((data == NULL) || (len == 0U) || (len > ROTCC_PACKET_SIZE)) {
        return rot_state_invalid_argument;
    }
    if (tx_mutex_get(&tx_enqueue_mutex, TX_WAIT_FOREVER) != TX_SUCCESS) {
        return rot_state_rotcc_tx_lock_fail;
    }

    tail = __atomic_load_n(&cc_msg.tail[ROTCC_DIR_TX], __ATOMIC_RELAXED);
    head = __atomic_load_n(&cc_msg.head[ROTCC_DIR_TX], __ATOMIC_ACQUIRE);
    if ((uint32_t)(tail - head) >= ROTCC_QUEUE_DEPTH) {
        (void)tx_mutex_put(&tx_enqueue_mutex);
        return rot_state_rotcc_tx_full;
    }

    packet = &cc_msg.packet_buf[ROTCC_DIR_TX][tail & ROTCC_QUEUE_MASK];
    packet->len = (uint16_t)len;
    memcpy(packet->data, data, len);
    __atomic_store_n(&cc_msg.tail[ROTCC_DIR_TX], tail + 1U, __ATOMIC_RELEASE);

    (void)tx_mutex_put(&tx_enqueue_mutex);
    (void)tx_event_flags_set(&cc_events, ROTCC_EVT_TX_SUBMIT, TX_OR);
    return rot_state_success;
}

bool rotcc_is_busy(void)
{
    uint32_t head = __atomic_load_n(&cc_msg.head[ROTCC_DIR_TX], __ATOMIC_ACQUIRE);
    uint32_t tail = __atomic_load_n(&cc_msg.tail[ROTCC_DIR_TX], __ATOMIC_ACQUIRE);

    return hal_cc_is_busy() || (head != tail);
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

