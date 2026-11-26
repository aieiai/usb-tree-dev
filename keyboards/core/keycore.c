#include "keycore.h"
#include "drv_hall.h"

#include "tx_api.h"
#include "tx_config.h"

#include "keyrt.h"
#include "keymap.h"
#include "keyevt.h"
#include "keyadv.h"
#include "keyprop.h"
#include "keyaxis.h"
#include "keycalib.h"
#include "keymagnet.h"
#include "keymonitor.h"
#include "keyfilter.h"
#include "keyhid.h"

#include "board.h"

#define TAG "core: "
#define KI_EN 0
#define KS_EN 0
#define KEYCORE_EVT_FRAME 0x1UL


#if KI_EN || KS_EN
static uint8_t cnt = 0;
static uint8_t done[SENSOR_TOTAL_SIZE] = {0};
static uint8_t view[SENSOR_TOTAL_SIZE] = {0};
static uint16_t sersor_buf[SENSOR_TOTAL_SIZE];
#endif

uint32_t core_freq_mhz = 0;

static TX_THREAD core_thread;
static uint8_t core_thread_stack[CORE_THREAD_STACK] __attribute__((aligned(8U)));
static TX_EVENT_FLAGS_GROUP core_events;

typedef struct {
    uint64_t ntf_tick;
    uint32_t ntf_time;

    uint64_t run_time;
    uint32_t *frame;
    uint16_t buf[2][KBD_KEY_MAX];
    uint8_t  buf_idx;
    uint8_t  is_mon : 1;
    uint8_t  is_filter : 1;
} __attribute__((aligned(4))) keycore_t;

static keycore_t g_keycore = {0};

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


static inline void keycore_find_ks(uint8_t ki, uint8_t si, uint16_t data)
{
#if KS_EN
    if (cnt == 0) {
        memset(view, 0xff, sizeof(view));
    }
    if ((!done[ki]) && g_keyevt[ki].trg == KEVT_TRG_U2D) {
        done[ki]++;
        view[ki] = si;
    }
#else
    (void)ki;
    (void)si;
    (void)data;
#endif
}

ATTR_RAMFUNC
static inline void keycore_process(uint16_t *p_buf)
{
    keyevt_t  *p_evt;
    keymag_t  *p_mag;
    keyattr_t *p_attr;
    const keyaxis_para_t *p_axis;

    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        p_evt  = &g_keyevt[ki];
        p_mag  = &g_keymag[ki];
        p_attr = &g_obkp.attr[ki];
        p_axis = p_evt->axis_para;

        p_evt->valp  = p_evt->val;
        p_evt->valdp = p_evt->vald;
        p_evt->val   = p_buf[ki];
        p_evt->vald  = p_evt->val - p_evt->valp;

        _keymag_pt2du(p_mag, p_axis, p_evt->val);
        _keyevt_md_detect(p_evt);
        _keyevt_pos_detect(p_evt, p_axis);
        _keyevt_trg_detect(p_evt);
        _keyevt_act_detect(p_evt, p_attr, ki);

        //if (keyrt_is_en(ki)) {
        //    keyrt_process(ki);
        //}
        //else {
        //}
    }
}

static void core_thread_entry(ULONG id)
{
    (void)id;
    keymap_t *p_map   = &g_keymap;
    keycore_t *p_core = &g_keycore;
    keyevt_t  *p_evt = NULL;
    keymag_t  *p_mag = NULL;
    keyattr_t *p_attr = NULL;
    const keyaxis_para_t *p_axis = NULL;
    uint64_t now = 0;
    uint8_t ki = 0;
    uint16_t cur_v = 0;
    uint16_t *p_buf = NULL;

    for (;;) {
        ULONG events = 0U;
        if (tx_event_flags_get(&core_events, KEYCORE_EVT_FRAME, TX_OR_CLEAR, &events, TX_WAIT_FOREVER) != TX_SUCCESS) {
            continue;
        }

        now = hpm_csr_get_core_cycle();
        p_core->ntf_time = (now - g_keycore.ntf_tick) / core_freq_mhz;

        ki = 0;
        cur_v = 0;
        p_buf = p_core->buf[p_core->buf_idx];
        p_core->buf_idx = !p_core->buf_idx;

        do {
            p_evt  = &g_keyevt[ki];
            p_mag  = &g_keymag[ki];
            p_attr = &g_obkp.attr[ki];
            p_axis = p_evt->axis_para;
            cur_v  = GET_ADC_VALUE(p_core->frame, p_map->ks_list[ki]);

            //keymonitor_lmt(ki, cur_v, p_buf[ki]);
            //p_buf[ki] = p_core->is_filter ? keyfilter_update(ki, cur_v) : cur_v;

            p_buf[ki] = cur_v;

            #if KI_EN
                keycore_find_ki(i, p_buf[ki]);
            #elif KS_EN
                keycore_find_ks(ki, i, p_buf[ki]);
            #endif

            p_evt->valp  = p_evt->val;
            p_evt->valdp = p_evt->vald;
            p_evt->val   = cur_v;
            p_evt->vald  = p_evt->val - p_evt->valp;

            _keymag_pt2du(p_mag, p_axis, p_evt->val);
            _keyevt_md_detect(p_evt);
            _keyevt_pos_detect(p_evt, p_axis);
            _keyevt_trg_detect(p_evt);
            _keyevt_act_detect(p_evt, p_attr, ki);

            //if (keyrt_is_en(ki)) {
            //    keyrt_process(ki);
            //}
            //else {
            //}
        } while (++ki < KBD_KEY_MAX);

        // 校准期间不可以处理发送键码
        if (_keycal_active()) {
            keyhid_suspend();
            keycal_post_adc(p_buf);
            continue;
        }
        
        keyhid_process();

        p_core->run_time = (hpm_csr_get_core_cycle() - now) / core_freq_mhz;
    }
}

static void keycore_recv_cb(uint32_t *p_buf)
{
    keycore_t *p_core = &g_keycore;
    p_core->ntf_tick = hpm_csr_get_core_cycle();
    p_core->frame    = p_buf;
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
    keyfilter_init();
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
