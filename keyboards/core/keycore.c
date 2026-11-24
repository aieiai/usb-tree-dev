#include "keycore.h"
#include "drv_hall.h"

#include "tx_api.h"
#include "tx_config.h"

#include "keymap.h"
#include "keyevt.h"
#include "keyadv.h"
#include "keyprop.h"
#include "keycalib.h"
#include "keymagnet.h"
#include "keymonitor.h"

#include "board.h"

#define TAG "core: "
#define KI_EN 0
#define KEYCORE_EVT_FRAME 0x1UL

static TX_THREAD core_thread;
static uint8_t core_thread_stack[CORE_THREAD_STACK] __attribute__((aligned(8U)));
static TX_EVENT_FLAGS_GROUP core_events;
static volatile uint32_t *core_frame = NULL;
static uint8_t adc_buf_idx = 0;
static uint16_t adc_buf[2][KBD_KEY_MAX];


#if KI_EN
static uint8_t cnt = 0;
static uint8_t done[SENSOR_TOTAL_SIZE] = {0};
static uint8_t view[SENSOR_TOTAL_SIZE] = {0};
static uint16_t sersor_buf[SENSOR_TOTAL_SIZE];
#endif

static void keycore_process_frame(volatile uint32_t *frame)
{
    adc_buf_idx = !adc_buf_idx;
    uint16_t *p_buf = adc_buf[adc_buf_idx];
    for (uint8_t ki = 0, i = 0; i < SENSOR_TOTAL_SIZE; i++) {
        ki = keymap_get_ki(i);
        if (KBD_CK_KI(ki) != true) {
            continue;
        }

        p_buf[ki] = GET_ADC_VALUE(frame, i);
#if KI_EN
        sersor_buf[i] = GET_ADC_VALUE(frame, i);
        if (cnt == 0) {
            memset(view, 0xff, sizeof(view));
        }
        if ((sersor_buf[i] < 12000) && (!done[i])) {
            done[i]++;
            view[i] = cnt++;
        }
#endif

        keyevt_updete(ki, p_buf[ki]);
        keymonitor_lmt(ki, p_buf[ki]);
    }

    if (keycalib_active()) {
        keycalib_post_adc(p_buf);
    }
}

static void core_thread_entry(ULONG id)
{
    (void)id;

    for (;;) {
        ULONG events = 0U;
        if (tx_event_flags_get(&core_events, KEYCORE_EVT_FRAME, TX_OR_CLEAR, &events, TX_WAIT_FOREVER) != TX_SUCCESS) {
            continue;
        }

        keycore_process_frame(core_frame);
    }
}

static void keycore_recv_cb(uint32_t *p_buf)
{
    core_frame = p_buf;
    (void)tx_event_flags_set(&core_events, KEYCORE_EVT_FRAME, TX_OR);
}

rotsta_t keycore_init(void)
{
    rotsta_t result = rotsta_success;
    
    result = keymap_init();
    if (result != rotsta_success) {
        printf(TAG "keymap init failed: 0x%08x, using defaults\r\n", (unsigned)result);
    }

    result = keyadv_init();
    if (result != rotsta_success) {
        printf(TAG "keyadv init failed: 0x%08x\r\n", (unsigned)result);
    }
    
    keyevt_init();

    sensor_matrix_init();
    sensor_matrix_recv_cb(keycore_recv_cb);

    if (tx_event_flags_create(&core_events, "core_evt") != TX_SUCCESS) {
        return rotsta_fail;
    }

    if (tx_thread_create(&core_thread, "core_thread", core_thread_entry, 0U, core_thread_stack, sizeof(core_thread_stack), CORE_THREAD_PRIORITY, CORE_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS) {
        return rotsta_fail;
    }

    return rotsta_success;
}
