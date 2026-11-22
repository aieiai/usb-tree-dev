/**
 * @Author: wj
 * @Brief:  key rapid trigger.
 * @Date:   2024/07/02
 */
#include "keyrt.h"
#include "key_sta.h"
#include "key_cfg.h"

#define S16_MIN     ( 0x8000 )
#define S16_MAX     ( 0x7FFF )

krt_t krt[KEY_NUM];


extern ksta_t ksta[KEY_NUM];


/**
 * @brief:  Reset the key rapid trigger state.
 * @param:  The index of the key.
 * @return: None
 *
 *          Reset the key rapid trigger state to uninitialized.
 */
void krt_sta_rst(uint8_t ki)
{
    if (krt[ki].rst == false)
    {
        return;
    }
    krt[ki].rst     = false;
    krt[ki].pd_dst  = S16_MIN;
    krt[ki].rd_dst  = S16_MAX;
    krt[ki].rnm_dst = 0;
    krt[ki].pnm_dst = 0;
    krt[ki].evt_ent = KRT_EVT_ENT_U2U;
    krt[ki].evt_trg = KRT_EVT_TRG_U2U;
}
void krt_sta_rst_force(uint8_t ki)
{
    krt[ki].rst = true;
    krt_sta_rst(ki);
}

/**
 * @brief:  Record the release depth dist
 * @param:  The index of the key
 * @param:  The current depth dist
 * @return: None
 *
 *          If the current depth dist is smaller than the previous one, the
 *          previous one will be updated.
 */
static __inline void krt_record_rd_dst(uint8_t ki, int16_t c_dst)
{
    if (krt[ki].rd_dst <= c_dst)
    {
        return;    
    }
    krt[ki].rnm_dst = 0;
    krt[ki].pd_dst  = S16_MIN;
    krt[ki].rd_dst  = c_dst;
    float dst       = c_dst + g_kcfg.attr[ki].rt.ssp_dst;
    krt[ki].pnm_dst = (dst > ksta[ki].type_para->t2b_dst) ? ksta[ki].type_para->t2b_dst : dst;
}

/**
 * @brief:  Record the press depth dist
 * @param:  The index of the key
 * @param:  The current depth dist
 * @return: None
 *
 *          If the current depth dist is smaller than the previous one, the
 *          previous one will be updated.
 */
static __inline void krt_record_pd_dst(uint8_t ki, int16_t c_dst)
{
    if (krt[ki].pd_dst >= c_dst)
    {
        return;
    }
    krt[ki].pnm_dst = 0;
    krt[ki].rd_dst  = S16_MAX;
    krt[ki].pd_dst  = c_dst;
    int16_t dst     = c_dst - g_kcfg.attr[ki].rt.ssr_dst;
    krt[ki].rnm_dst = (dst < 0) ? 0 : dst;
}


/**
 * @brief:  Process the key rapid trigger event.
 * @param:  The index of the key.
 * @return: None
 *
 *          This function is used to process the key rapid trigger event.
 *          If the key is pressed from up to up, the krt.rst will be set to true.
 *          If the key is pressed from up to down, the krt.evt_ent will be set to
 *          KRT_EVT_ENT_D2D, and the krt.evt_trg will be set to KRT_EVT_TRG_U2D.
 *          The press depth dist will be recorded.
 *          If the key is pressed from down to down, the krt.evt_trg will be set to
 *          KRT_EVT_TRG_D2D, and the raise depth dist will be recorded.
 *          If the key is pressed from down to up, the krt.evt_trg will be set to
 *          KRT_EVT_TRG_D2U, and the press depth dist will be recorded.
 */
void krt_process(uint8_t ki)
{
    if (ksta[ki].evt_ent == KSTA_EVT_ENT_U2U)
    {
        krt_sta_rst(ki);
        return;
    }

    int16_t c_dst = ksta[ki].dst;
    if (krt[ki].evt_ent == KRT_EVT_ENT_U2U)
    {
        krt[ki].rst     = true;
        krt[ki].evt_ent = KRT_EVT_ENT_D2D;
        krt[ki].evt_trg = KRT_EVT_TRG_U2D;
        krt_record_pd_dst(ki, c_dst);
        return;
    }
    
    if (krt[ki].evt_trg < KRT_EVT_TRG_D2D)
    {
        krt_record_rd_dst(ki, c_dst);
        krt[ki].evt_trg = (krt[ki].pnm_dst <= c_dst) ? KRT_EVT_TRG_U2D : KRT_EVT_TRG_U2U;
    }
    else
    {
        krt_record_pd_dst(ki, c_dst);
        krt[ki].evt_trg = (krt[ki].rnm_dst >= c_dst) ? KRT_EVT_TRG_D2U : KRT_EVT_TRG_D2D;
    }
}

/**
 * @brief:  Initialize the key rapid trigger module.
 * @param:  None
 * @return: The pointer to the key rapid trigger module.
 *
 *          This function is used to initialize the key rapid trigger module.
 *          It will reset all the key's rapid trigger state to uninitialized.
 */
krt_t *krt_init(void)
{
    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        krt[ki].rst = true;
        krt_sta_rst(ki);
    }
    return krt;
}

/**
 * @brief:  Deinitialize the key rapid trigger module.
 * @param:  None
 * @return: None
 *
 *          This function is used to deinitialize the key rapid trigger module.
 *          It will reset all the key's rapid trigger state to uninitialized.
 */
void krt_deinit(void)
{
    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        krt[ki].rst = true;
        krt_sta_rst(ki);
    }
}

