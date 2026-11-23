#include "keycore.h"
#include "drv_hall.h"

#include "tx_api.h"
#include "tx_config.h"

#include "keymap.h"
#include "keyevt.h"
#include "keyadv.h"
#include "keyprop.h"
#include "keymagnet.h"

#include "board.h"

#define TAG "core: "
#define KI_EN 0
#define KEYCORE_EVT_FRAME 0x1UL

static TX_THREAD core_thread;
static uint8_t core_thread_stack[CORE_THREAD_STACK] __attribute__((aligned(8U)));
static TX_EVENT_FLAGS_GROUP core_events;
static volatile uint32_t *core_frame = NULL;
static uint16_t adc_buf[KBD_KEY_MAX];

static uint8_t ks2ki[SENSOR_TOTAL_SIZE] = {
    22,  24,  28,  25,  29,  27,  15,  16,  19,  17,  21,  18,  
    23,  39,  56,  62,  67,  64,  59,  60,  35,  255, 61,  4,   
    8,   10,  13,  11,  14,  12,  0,   1,   5,   2,   6,   3,   
    9,   52,  65,  26,  66,  63,  58,  255, 20,  255, 7,   255, 
    50,  51,  55,  53,  57,  54,  44,  45,  48,  46,  49,  47,  
    37,  38,  42,  40,  43,  41,  30,  31,  34,  32,  36,  33,  
};

#if KI_EN
static uint8_t cnt = 0;
static uint8_t done[SENSOR_TOTAL_SIZE] = {0};
static uint8_t view[SENSOR_TOTAL_SIZE] = {0};
static uint16_t sersor_buf[SENSOR_TOTAL_SIZE];
#endif

static void keycore_process_frame(volatile uint32_t *frame)
{
    for (uint8_t ki = 0, i = 0; i < SENSOR_TOTAL_SIZE; i++) {
        ki = (ks2ki[i]);
        if (KBD_CK_KI(ki) != true) {
            continue;
        }

        adc_buf[ki] = GET_ADC_VALUE(frame, i);
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

        keyevt_updete(ki, adc_buf[ki]);



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
    rotsta_t state = rotsta_success;
    
    state = keymap_init();
    if (state != rotsta_success) {
        printf(TAG "keymap init failed: 0x%08x, using defaults\r\n", (unsigned)state);
    }

    state = keymag_init();
    if (state != rotsta_success) {
        printf(TAG "keymag init failed: 0x%08x, using defaults\r\n", (unsigned)state);
    }
    
    state = keyadv_init();
    if (state != rotsta_success) {
        printf(TAG "keyadv init failed: 0x%08x\r\n", (unsigned)state);
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
