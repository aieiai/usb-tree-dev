#include "keymonitor.h"
#include "keymagnet.h"
#include "kbd_config.h"
#include "keycore.h"

#include "hpm_clock_drv.h"

#define TAG "keymonitor: "
#define WAIT_TICK MS2TK(2000)

typedef struct {
    uint8_t  rst;
    uint16_t max_mval;
    uint32_t wait_tick;
    uint64_t run_us;
    uint16_t dta[KBD_KEY_MAX];
    uint16_t dif[KBD_KEY_MAX];
    uint16_t mvol[KBD_KEY_MAX];
    keymag_lmt_t lmt[KBD_KEY_MAX];
} __attribute__((aligned(4))) keymon_t;

static uint64_t run_tick = 0;
static keymon_t g_keymon = {
    .rst = 1,
};

void keymonitor_lmt(uint8_t ki, uint16_t data, uint16_t data_pre)
{
    keymon_t *p_mon = &g_keymon;

    if (ki == 0U) {
        uint64_t now = hpm_csr_get_core_cycle();
        p_mon->run_us = (now - run_tick) / core_freq_mhz;
        run_tick = now;

        if (p_mon->wait_tick != 0U) {
            p_mon->wait_tick--;
        }
    }

    if (p_mon->wait_tick != 0U) {
        return;
    }

    if (p_mon->rst) {
        p_mon->rst = 0;
        p_mon->wait_tick = WAIT_TICK;
        p_mon->max_mval = 0;
        for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
            p_mon->lmt[ki].min = UINT16_MAX;
            p_mon->lmt[ki].max = 0U;
            p_mon->dta[ki] = 0U;
        }
        return;
    }

    /* 相邻两周期最大差值：有符号求差再取绝对值，避免 uint16 回绕 */
    {
        int32_t  d  = (int32_t)data - (int32_t)data_pre;
        uint16_t ad = (uint16_t)((d < 0) ? -d : d);

        if (ad > p_mon->dta[ki]) {
            p_mon->dta[ki] = ad;
        }
    }

    if (data < p_mon->lmt[ki].min) {
        p_mon->lmt[ki].min = data;
    }
    if (data > p_mon->lmt[ki].max) {
        p_mon->lmt[ki].max = data;
    }

    p_mon->dif[ki]  = p_mon->lmt[ki].max - p_mon->lmt[ki].min;
    p_mon->mvol[ki] = (uint16_t)KBD_ADC2MVOL(p_mon->dif[ki]);

    if (p_mon->mvol[ki] > p_mon->max_mval) {
        p_mon->max_mval = p_mon->mvol[ki];
    }
}

void keymonitor_rst(void)
{
    g_keymon.rst = true;
}
