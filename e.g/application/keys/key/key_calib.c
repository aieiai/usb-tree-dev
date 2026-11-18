/**
 * @Author: wj
 * @Brief:  key calib processing.
 * @Date:   2024/07/02
 */
#include "key_calib.h"
#include "key_sta.h"
#include "key_cfg.h"

kcal_t kcal;

kcal_res_t kcal_res = {
    .save = false,
    .res  = KCAL_RES_EXIT,
};

kcal_step_t kcal_step = {
    .fst = false,
    .now = KCAL_STEP_IDLE,
    .pre = KCAL_STEP_IDLE,
    .cmd = KCAL_STEP_IDLE,
};

kcal_info_t kcal_info = {
    .dcnt = 0,
    .dbit = 0,
    .done = 0
};

kcal_mnt_t kcal_mnt[KEY_NUM];

extern ksta_t ksta[KEY_NUM];
/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline void kcalib_clear(void)
{
    kcal.info->dcnt = 0;
    kcal.info->dbit = 0;
    kcal.info->done = 0;

    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        kcal.mnt[ki].val.max = 0x00;
        kcal.mnt[ki].val.min = 0xffff;
        kcal.mnt[ki].pass = 0;
    }
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline void kcalib_handle(void)
{
    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        uint16_t dif;
        uint16_t val  = ksta[ki].valn;
        ktype_t ktype = g_kcfg.attr[ki].type;

        if (val < kcal.mnt[ki].val.min)
        {
            kcal.mnt[ki].val.min = val;
        }
        else if (val > kcal.mnt[ki].val.max)
        {
            kcal.mnt[ki].val.max = val;
        }
        dif = kcal.mnt[ki].val.max - kcal.mnt[ki].val.min;

        if (BIT_GET(kcal.info->dbit, (0x01 << ki)) != false) // pass
        {
            bool is_over = dif > g_ktp_para[ktype].dif.max;
            if (!is_over)
            {
                continue;
            }
            kcal.mnt[ki].pass = 0;
            kcal.info->dcnt  -= 1;
            kcal.info->done   = false;
            BIT_CLR(kcal.info->dbit, (0x01 << ki));
        }
        else // not pass
        {
            bool is_pass = ((dif >= g_ktp_para[ktype].dif.min) && (dif <= g_ktp_para[ktype].dif.max) \
                            && (kcal.mnt[ki].val.top >= g_ktp_para[ktype].top.min) && (kcal.mnt[ki].val.top <= g_ktp_para[ktype].top.max) \
                            && (kcal.mnt[ki].val.bot >= g_ktp_para[ktype].bot.min) && (kcal.mnt[ki].val.bot <= g_ktp_para[ktype].bot.max));
            if (!is_pass)
            {
                continue;
            }
            if (kcal.mnt[ki].pass < 100)
            {
                kcal.mnt[ki].pass++;
                continue;
            }
            kcal.info->dcnt += 1;
            BIT_SET(kcal.info->dbit, (0x01 << ki));
            if (kcal.info->dcnt == KEY_NUM)
            {
                kcal.info->done = true;
                break;
            }
        }
    }
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline bool kcalib_data_reload(void)
{
    bool is_save = false;
    for (uint8_t ki = 0; ki < KEY_NUM; ki++) // update key attr
    {
        if (BIT_GET(kcal.info->dbit, (0x01 << ki))) // pass
        {
            is_save = true;
            g_kcfg.attr[ki].val.top = kcal.mnt[ki].val.top;
            g_kcfg.attr[ki].val.bot = kcal.mnt[ki].val.bot;
        }
    }

    if (is_save)
    {
        // g_ksv.time = get_tick();
        g_ksv.sta = KSV_STA_WAIT_SAVE;
    }
    return is_save;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
kcal_res_type_t kcalib_process(void)
{
    kcal.step->fst = (kcal.step->now != kcal.step->pre) ? true : false; // step change
    kcal.step->pre = kcal.step->now; // update step pre
    kcal.step->now = kcal.step->cmd; //update step
    kcal.res->res  = KCAL_RES_KEEP;
    switch (kcal.step->now)
    {
        case KCAL_STEP_IDLE:
            break;

        case KCAL_STEP_ABORT:
            kcal.step->cmd = KCAL_STEP_EXIT;
            break;

        case KCAL_STEP_START:
        case KCAL_STEP_RESTART:
            kcalib_clear();
            kcal.step->cmd = KCAL_STEP_CALING;
            break;

        case KCAL_STEP_CALING:
            kcalib_handle();
            break;

        case KCAL_STEP_DONE:
            kcal.res->save = kcalib_data_reload();
            kcal.step->cmd = KCAL_STEP_EXIT;
            break;

        case KCAL_STEP_EXIT:
            kcalib_clear();
            kcal.res->res  = (kcal.step->pre == KCAL_STEP_ABORT) ? KCAL_RES_EXIT : ((kcal.res->save == false) ? KCAL_RES_EXIT_DONE : KCAL_RES_EXIT_RELOAD);
            kcal.step->cmd = kcal.step->pre = kcal.step->now = KCAL_STEP_IDLE;
            break;

        default:
            break;
    }
    return kcal.res->res;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
kcal_t *kcalib_init(void)
{
    kcal.info = &kcal_info;
    kcal.mnt  = kcal_mnt;
    kcal.res  = &kcal_res;
    kcal.step = &kcal_step;

    kcalib_clear();

    return &kcal;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void kcalib_deinit(void)
{
}
