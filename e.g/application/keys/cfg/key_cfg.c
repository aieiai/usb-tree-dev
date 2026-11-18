/**
 * @Author: wj
 * @Brief:  key config.
 * @Date:   2024/07/02
 */
#include "key_cfg.h"



kcfg_t g_kcfg;

ksv_t g_ksv = { 
    .sta = KSV_STA_NOINIT, 
    .time = 0 
};

const hid_kc_t g_kmap_def[KLAYER_MAX][KEY_NUM] = {
    {HID_KC_LEFT_ALT, HID_KC_LEFT_GUI, HID_KC_LEFT_CONTROL, HID_KC_CAPS_LOCK, 
    HID_KC_LEFT_SHIFT, HID_KC_ESC, HID_KC_TAB, HID_KC_1, HID_KC_2, HID_KC_3}, //0
    {HID_KC_C, HID_KC_X, HID_KC_Z, HID_KC_D, HID_KC_S, HID_KC_A, HID_KC_Q, HID_KC_W, HID_KC_E, HID_KC_4}, //1
//    {HID_KC_8, HID_KC_9, HID_KC_0, HID_KC_I, HID_KC_O, HID_KC_U, HID_KC_Y, HID_KC_T, HID_KC_5, HID_KC_6, HID_KC_7}, //2
};

kcal_lmt_t tmpv[KEY_NUM] = {
    { .top = 31876, .bot = 17729, },
    { .top = 32972, .bot = 17969, },
    { .top = 33029, .bot = 19628, },
    { .top = 33875, .bot = 18525, },
    { .top = 33113, .bot = 20725, },
    { .top = 32945, .bot = 19618, },
    { .top = 34104, .bot = 19915, },
    { .top = 32853, .bot = 18295, },
    { .top = 33326, .bot = 19814, },
    { .top = 33437, .bot = 18038, },
};

const ktp_para_t g_ktp_para[KTYPE_MAX] = {
    //KTYPE_GDL_WHT
    {
        .t2b_dst = KTYPE_GDL_WHT_STROKE,
        .b2h_dst = BOARD_THICKNESS + BOARD_TO_HALL_DIST,
        .t2h_dst = BOARD_THICKNESS + BOARD_TO_HALL_DIST + KTYPE_GDL_WHT_STROKE,
        .top.max = 50000,
        .top.min = 0,
        .bot.max = 50000,
        .bot.min = 0,
        .dif.max = 18000,
        .dif.min = 10000,
    },
    //KTYPE_GDL_JADE
    {
        .t2b_dst = KTYPE_GDL_JADE_STROKE,
        .b2h_dst = BOARD_THICKNESS + BOARD_TO_HALL_DIST,
        .t2h_dst = BOARD_THICKNESS + BOARD_TO_HALL_DIST + KTYPE_GDL_JADE_STROKE,
        .top.max = 50000,
        .top.min = 0,
        .bot.max = 50000,
        .bot.min = 0,
        .dif.max = 18000,
        .dif.min = 10000,
    },
    //KTYPE_TTC_WCW
    {
        .t2b_dst = KTYPE_TTC_WCW_STROKE,
        .b2h_dst = BOARD_THICKNESS + BOARD_TO_HALL_DIST,
        .t2h_dst = BOARD_THICKNESS + BOARD_TO_HALL_DIST + KTYPE_TTC_WCW_STROKE,
        .top.max = 50000,
        .top.min = 0,
        .bot.max = 50000,
        .bot.min = 0,
        .dif.max = 18000,
        .dif.min = 10000,
    },
};




/**
 * @brief:  
 * @param:  None
 * @return: None
 */
const hid_kc_t *kmap_get_kc(klayer_type_t klt)
{
    if (klt >= KLAYER_MAX)
    {
        return NULL;
    }
    return g_kcfg.info.map[klt];
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
static kattr_t *kattr_init(void)
{
    //TODO:  key attr init.
    
    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        g_kcfg.attr[ki].type = KTYPE_DFT;
    
        // g_kcfg.attr[ki].val.top = 1906;
        // g_kcfg.attr[ki].val.bot = 1341;
        
        g_kcfg.attr[ki].val = tmpv[ki];

        g_kcfg.attr[ki].nom.p_dst = MM2UM(2.5f);
        g_kcfg.attr[ki].nom.r_dst = g_kcfg.attr[ki].nom.p_dst - MM2UM(0.1f);
        
        g_kcfg.attr[ki].rt.ssp_dst = MM2UM(0.8f);
        g_kcfg.attr[ki].rt.ssr_dst = MM2UM(0.8f);

        g_kcfg.attr[ki].rt.en      = true;
        g_kcfg.attr[ki].type_op    = KOP_TYPE_RT;

    }
    
    return g_kcfg.attr;
}
static void kb_info_init(void)
{
    g_kcfg.info.nkro = true;
    memcpy(g_kcfg.info.map, g_kmap_def, sizeof(g_kcfg.info.map));
}


void kcfg_save_cfg(void)
{
    g_ksv.sta = KSV_STA_INIT;
}


/**
 * @brief:  
 * @param:  None
 * @return: None
 */
kcfg_t *kcfg_init(void)
{
    kcfg_save_cfg();
    kb_info_init();
    kattr_init();
    return &g_kcfg;
}

