#include "keymonitor.h"
#include "keymagnet.h"
#include "kbd_config.h"
#include "keycore.h"

#include "hpm_clock_drv.h"

typedef struct {
    uint8_t rst;
    uint16_t max_mval;
    uint32_t wait_tick;
    uint64_t run_us;
    uint16_t dif[KBD_KEY_MAX];
    uint16_t mvol[KBD_KEY_MAX];
    keymag_lmt_t lmt[KBD_KEY_MAX];
} __attribute__((aligned(4))) keymon_t;

static uint64_t run_tick = 0;
static keymon_t g_keymon;

void keymonitor_lmt(uint8_t ki, uint16_t data)
{
    run_tick = hpm_csr_get_core_cycle();
    keymon_t *p_mon = &g_keymon;
    if (!KBD_CK_KI(ki) || (p_mon->wait_tick != 0)) {
        p_mon->wait_tick--;
        return;
    }

    if (p_mon->rst) {
        p_mon->rst = 0;
        p_mon->wait_tick = MS2TK(5000);
        p_mon->max_mval = 0;
        for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
            keymon_t *p = &g_keymon;
            p->lmt[ki].min = UINT16_MAX;
            p->lmt[ki].max = 0U;
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
    p_mon->run_us = (hpm_csr_get_core_cycle() - run_tick) / (clock_get_frequency(clock_cpu0) / 1000000);
}
void keymonitor_rst(void)
{
    g_keymon.rst = true;
}
