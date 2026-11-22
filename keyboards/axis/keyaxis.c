#include "keyaxis.h"
#include "kbd_config.h"


const keyaxis_para_t g_keyaxis_para[KAXIS_MAX] = {
    {
        .t2b_dst = KAXIS_BB_STROKE,
        .b2h_dst = KBD_PCB_THICKNESS + KBD_PCB_TO_HALL_DIST,
        .t2h_dst = KBD_PCB_THICKNESS + KBD_PCB_TO_HALL_DIST + KAXIS_BB_STROKE,
        .top.max = 50000,
        .top.min = 0,
        .bot.max = 50000,
        .bot.min = 0,
        .dif.max = 18000,
        .dif.min = 10000,
    },
};