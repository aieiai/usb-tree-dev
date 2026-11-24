#include "keymonitor.h"
#include "keymagnet.h"
#include "kbd_config.h"

typedef struct {
    uint16_t data;
    uint16_t dif;
    uint16_t mvol;
    keymag_lmt_t lmt;
} __attribute__((aligned(4))) keymon_t;

static uint8_t mon_rst = true;
keymon_t g_keymon[KBD_KEY_MAX];

void keymonitor_lmt(uint8_t ki, uint16_t data)
{
    keymon_t *p_mon = &g_keymon[ki];
    if (!KBD_CK_KI(ki)) {
        return;
    }

    if (mon_rst) {
        mon_rst = false;
        for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
            p_mon = &g_keymon[ki];
            p_mon->lmt.min = UINT16_MAX;
            p_mon->lmt.max = 0U;
        }
    }

    p_mon->data = data;

    if (p_mon->data < p_mon->lmt.min) {
        p_mon->lmt.min = p_mon->data;
    }
    else if (p_mon->data > p_mon->lmt.max) {
        p_mon->lmt.max = p_mon->data;
    }

    p_mon->dif  = p_mon->lmt.max - p_mon->lmt.min;
    p_mon->mvol = (uint16_t)KBD_ADC2MVOL(p_mon->dif);
}
void keymonitor_rst(void)
{
    mon_rst = true;
}