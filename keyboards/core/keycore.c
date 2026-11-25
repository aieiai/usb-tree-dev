#include "keycore.h"
#include "drv_hall.h"

#include "tx_api.h"
#include "tx_config.h"

#include "keyrt.h"
#include "keymap.h"
#include "keyevt.h"
#include "keyadv.h"
#include "keyprop.h"
#include "keycalib.h"
#include "keymagnet.h"
#include "keymonitor.h"
#include "keyhid.h"

#include "board.h"

#define TAG "core: "
#define KI_EN 0
#define KEYCORE_EVT_FRAME 0x1UL


#if KI_EN
static uint8_t cnt = 0;
static uint8_t done[SENSOR_TOTAL_SIZE] = {0};
static uint8_t view[SENSOR_TOTAL_SIZE] = {0};
static uint16_t sersor_buf[SENSOR_TOTAL_SIZE];
#endif

uint32_t core_freq_mhz = 0;

static TX_THREAD core_thread;
static uint8_t core_thread_stack[CORE_THREAD_STACK] __attribute__((aligned(8U)));
static TX_EVENT_FLAGS_GROUP core_events;
static volatile uint32_t *core_frame = NULL;
static uint8_t adc_buf_idx = 0;
static uint16_t adc_buf[2][KBD_KEY_MAX];

uint8_t trg_ty = 0;
static void keycore_process(void)
{
    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        keyevt_t *p_evt = &g_keyevt[ki];
        if (keyrt_is_en(ki)) {
            keyrt_process(ki);
        }
        else {
            p_evt->trg = keyevt_trg_detect(ki);
        }
    }
}

static inline void keycore_find_ki(uint8_t si, uint16_t data)
{
#if KI_EN
    sersor_buf[si] = data;
    if (cnt == 0) {
        memset(view, 0xff, sizeof(view));
    }
    if ((sersor_buf[si] < 12000) && (!done[si])) {
        done[si]++;
        view[si] = cnt++;
    }
#else
    (void)si;
    (void)data;
#endif
}

static void core_thread_entry(ULONG id)
{
    (void)id;

    for (;;) {
        ULONG events = 0U;
        if (tx_event_flags_get(&core_events, KEYCORE_EVT_FRAME, TX_OR_CLEAR, &events, TX_WAIT_FOREVER) != TX_SUCCESS) {
            continue;
        }

        adc_buf_idx = !adc_buf_idx;
        uint16_t *p_buf = adc_buf[adc_buf_idx];
        for (uint8_t ki = 0, i = 0; i < SENSOR_TOTAL_SIZE; i++) {
            ki = keymap_get_ki(i);
            
            if (KBD_CK_KI(ki) != true) {
                continue;
            }

            p_buf[ki] = GET_ADC_VALUE(core_frame, i);

            keycore_find_ki(i, p_buf[ki]);
            keyevt_updete(ki, p_buf[ki]);
            keymonitor_lmt(ki, p_buf[ki]);
        }

        // 校准期间不可以处理发送键码
        if (keycalib_active()) {
            keyhid_suspend();
            keycalib_post_adc(p_buf);
            continue;
        }
        
        keycore_process();
        keyhid_process();
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
    keyhid_init();

    sensor_matrix_init();
    sensor_matrix_recv_cb(keycore_recv_cb);

    core_freq_mhz = hpm_core_clock / 1000000;
    if (tx_event_flags_create(&core_events, "core_evt") != TX_SUCCESS) {
        return rotsta_fail;
    }

    if (tx_thread_create(&core_thread, "core_thread", core_thread_entry, 0U, core_thread_stack, sizeof(core_thread_stack), CORE_THREAD_PRIORITY, CORE_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS) {
        return rotsta_fail;
    }

    return rotsta_success;
}
