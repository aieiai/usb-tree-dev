#include "keyaxis.h"
#include "kbd_config.h"

const kaxis_ty g_kbd_axis_support[] = {
    KAXIS_BB,
    KAXIS_GDL,
    KAXIS_MAX, /* sentinel */
};

const kaxis_udef_ty g_kbd_axis_udef_support[] = {
    KAXIS_UDEF0,
    KAXIS_UDEF1,
    KAXIS_UDEF2,
    KAXIS_UDEF3,
};

const keyaxis_para_t g_keyaxis_para[KAXIS_MAX] = {
    {
        .t2b_dst = KAXIS_STROKE_BB,
        .b2h_dst = KBD_PCB_THICKNESS + KBD_PCB_TO_HALL_DIST,
        .t2h_dst = KBD_PCB_THICKNESS + KBD_PCB_TO_HALL_DIST + KAXIS_STROKE_BB,
        .top.max = 50000,
        .top.min = 0,
        .bot.max = 50000,
        .bot.min = 0,
        .dif.max = 18000,
        .dif.min = 10000,
    },
};