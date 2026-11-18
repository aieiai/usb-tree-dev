/**
 * @Author: wj
 * @Brief:  key real-time status.
 * @Date:   2024/07/02
 */
#include "key_sta.h"
#include "hal.h"

ksta_t ksta[KEY_NUM];

/**
 * @brief:  Detect the action event of the key.
 * @param:  The index of the key.
 * @return: The action event of the key.
 *
 *          Detect the action event of the key by the enter event and action state.
 *          The action event can be one of the following:
 *            - KSTA_EVT_ACT_IDLE: Idle.
 *            - KSTA_EVT_ACT_UP: Up.
 *            - KSTA_EVT_ACT_SC: Click.
 *            - KSTA_EVT_ACT_SCL: Click long.
 *            - KSTA_EVT_ACT_SCLTO: Click long timeout.
 *            - KSTA_EVT_ACT_DC: Double click.
 *            - KSTA_EVT_ACT_DCL: Double click long.
 *            - KSTA_EVT_ACT_DCLTO: Double click long timeout.
 */
static __inline ksta_evt_act_t ksta_act_detect(uint8_t ki)
{
    switch (ksta[ki].act_sta)
    {
        case KACT_STA_IDLE:
        {
            if (ksta[ki].evt_ent == KSTA_EVT_ENT_U2U)
            {
                ksta[ki].evt_act = KSTA_EVT_ACT_IDLE; /* idle */
                break;
            }
            else
            {
                ksta[ki].act_tk  = 0;
                ksta[ki].act_sta = KACT_STA_SC;
            }
        }
        // break;
        case KACT_STA_SC:
        {
            if (ksta[ki].evt_ent == KSTA_EVT_ENT_U2U)
            {
                ksta[ki].act_tk  = 0;
                ksta[ki].act_sta = KACT_STA_SCN;
            }
            else
            {
                if (ksta[ki].act_tk >= g_kcfg.attr[ki].tk.sclt)
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_SCL;
                }
                else
                {
                    ksta[ki].act_tk++;
                }
            }
        }
        break;
        case KACT_STA_SCN:
        {
            if (ksta[ki].evt_ent == KSTA_EVT_ENT_D2D)
            {
                ksta[ki].act_tk  = 0;
                ksta[ki].act_sta = KACT_STA_DC;
            }
            else
            {
                if (ksta[ki].act_tk >= g_kcfg.attr[ki].tk.dcit)
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_IDLE;
                    ksta[ki].evt_act = KSTA_EVT_ACT_SC; /* click */
                }
                else
                {
                    ksta[ki].act_tk++;
                }
            }
        }
        break;
        case KACT_STA_DC:
        {
            if (ksta[ki].evt_ent == KSTA_EVT_ENT_U2U)
            {
                ksta[ki].act_tk  = 0;
                ksta[ki].act_sta = KACT_STA_IDLE;
                ksta[ki].evt_act = KSTA_EVT_ACT_DC; /* double click */
            }
            else
            {
                if (ksta[ki].act_tk >= g_kcfg.attr[ki].tk.dclt)
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_DCL;
                }
                else
                {
                    ksta[ki].act_tk++;
                }
            }
        }
        break;
        case KACT_STA_SCL:
        {
            if (ksta[ki].evt_ent == KSTA_EVT_ENT_D2D)
            {
                if (ksta[ki].act_tk >= g_kcfg.attr[ki].tk.sclto)
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_UP;
                    ksta[ki].evt_act = KSTA_EVT_ACT_SCLTO; /* click long timeout */
                }
                else
                {
                    ksta[ki].act_tk++;
                }
            }
            else
            {
                if (ksta[ki].act_tk >= g_kcfg.attr[ki].tk.sclto)
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_IDLE;
                }
                else
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_IDLE;
                    ksta[ki].evt_act = KSTA_EVT_ACT_SCL; /* click long */
                }
            }
        }
        break;
        case KACT_STA_DCL:
        {
            if (ksta[ki].evt_ent == KSTA_EVT_ENT_D2D)
            {
                if (ksta[ki].act_tk >= g_kcfg.attr[ki].tk.dclto)
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_UP;
                    ksta[ki].evt_act = KSTA_EVT_ACT_DCLTO; /* double click long timeout */
                }
                else
                {
                    ksta[ki].act_tk++;
                }
            }
            else
            {
                if (ksta[ki].act_tk >= g_kcfg.attr[ki].tk.dclto)
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_IDLE;
                }
                else
                {
                    ksta[ki].act_tk  = 0;
                    ksta[ki].act_sta = KACT_STA_IDLE;
                    ksta[ki].evt_act = KSTA_EVT_ACT_DCL; /* double click long */
                }
            }
        }
        break;
        case KACT_STA_UP:
        {
            if (ksta[ki].evt_ent == KSTA_EVT_ENT_U2U)
            {
                ksta[ki].act_sta = KACT_STA_IDLE;
                ksta[ki].evt_act = KSTA_EVT_ACT_UP; /* up */
            }
        }
        break;
    }
    return ksta[ki].evt_act;
}

/**
 * @brief:  Detect the move direction of the key.
 * @param:  The index of the key.
 * @return: The move direction of the key.
 *
 *          Detect the move direction of the key by comparing the current value
 *          with the previous value. The move direction can be one of the following:
 *            - KSTA_EVT_MD_U: Move up.
 *            - KSTA_EVT_MD_D: Move down.
 *            - KSTA_EVT_MD_S: Stay still.
 */
static __inline ksta_evt_md_t ksta_md_detect(uint8_t ki)
{
    return ((ksta[ki].valc > 0)   ? KSTA_EVT_MD_U
            : (ksta[ki].valc < 0) ? KSTA_EVT_MD_D
                                  : KSTA_EVT_MD_S);
}

/**
 * @brief:  Detect the trigger event of the key.
 * @param:  The index of the key.
 * @return: The trigger event of the key.
 *
 *          Detect the trigger event of the key by comparing the current value
 *          with the previous value and threshold. The trigger event can be one
 *          of the following:
 *            - KSTA_EVT_TRG_U2U: From up to up.
 *            - KSTA_EVT_TRG_U2D: From up to down.
 *            - KSTA_EVT_TRG_D2D: From down to down.
 *            - KSTA_EVT_TRG_D2U: From down to up.
 */
static __inline ksta_evt_trg_t ksta_trg_detect(uint8_t ki)
{
    return ((ksta[ki].evt_trg < KSTA_EVT_TRG_D2D)
                ? ((ksta[ki].valn <= ksta[ki].pp) ? KSTA_EVT_TRG_U2D : KSTA_EVT_TRG_U2U)
                : ((ksta[ki].valn >= ksta[ki].rp) ? KSTA_EVT_TRG_D2U : KSTA_EVT_TRG_D2D));
}

/**
 * @brief:  Detect the enter event of the key.
 * @param:  The index of the key.
 * @return: The enter event of the key.
 *
 *          Detect the enter event of the key by the trigger event. The enter event
 *          can be one of the following:
 *            - KSTA_EVT_ENT_D2D: The key is pressed from down to down.
 *            - KSTA_EVT_ENT_U2U: The key is pressed from up to up.
 */
static __inline ksta_evt_ent_t ksta_ent_detect(uint8_t ki)
{
    return ((ksta[ki].evt_trg < KSTA_EVT_TRG_D2D)
                ? KSTA_EVT_ENT_U2U
                : KSTA_EVT_ENT_D2D);
}

/**
 * @brief:  Detect the position event of the key.
 * @param:  The index of the key.
 * @return: The position event of the key.
 *
 *          Detect the position event of the key by comparing the current value
 *          with the threshold. The position event can be one of the following:
 *            - KSTA_EVT_POS_TOP: The key is released and the position is at the top.
 *            - KSTA_EVT_POS_BOT: The key is pressed and the position is at the bottom.
 *            - KSTA_EVT_POS_MID: The key is pressed and the position is between the top and the bottom.
 */
static __inline ksta_evt_pos_t ksta_pos_detect(uint8_t ki)
{
    return ((ksta[ki].dst <= 50) ? KSTA_EVT_POS_TOP
            : (ksta[ki].dst >= ksta[ki].type_para->t2b_dst)
                ? KSTA_EVT_POS_BOT
                : KSTA_EVT_POS_MID);
}

/**
 * @brief:  Update the coefficient that calculates the distance between key.
 * @param:  The index of the key.
 * @return: None
 */
void ksta_update_factor(uint8_t ki)
{
    float topv  = g_kcfg.attr[ki].val.top;
    float botv  = g_kcfg.attr[ki].val.bot;
    float t2hdm = ksta[ki].type_para->t2h_dst / 1000.0f;
    float b2hdm = ksta[ki].type_para->b2h_dst / 1000.0f;
    float t2bv  = topv - botv;
    float k     = (HAL_ADC_VEF / (float)((0x01 << HAL_ADC_BIT) - 1.0f) * t2bv) / ((1.0f / t2hdm) - (1.0f / b2hdm));
    float b     = (HAL_ADC_VEF / (float)((0x01 << HAL_ADC_BIT) - 1.0f) * topv) - (k / t2hdm);
    ksta[ki].k  = (1000.0f * 1000.0f / (HAL_ADC_VEF * 1000.0f) * (float)((0x01 << HAL_ADC_BIT) - 1.0f)) * k;
    ksta[ki].b  = (1000.0f * (float)((0x01 << HAL_ADC_BIT) - 1.0f) / (HAL_ADC_VEF * 1000.0f)) * b;
}

/**
 * @brief:  Convert the point location to the distance from the top of the key to the bottom.
 * @param:  The index of the key.
 * @param:  The point location to convert.
 * @return: The distance from the top of the key to the bottom.
 *
 *          The distance is calculated by the following formula.
 *          distance = (point - b) / k
 *          where b is the y-intercept of the line and k is the slope of the line.
 *          The line is defined by the key's attribute, such as the top and bottom values.
 */
int16_t ksta_pt2dst(uint8_t ki, uint16_t pt)
{
    return (int16_t)((float)ksta[ki].type_para->t2h_dst - (ksta[ki].k / ((float)pt - ksta[ki].b)));
}

/**
 * @brief:  Convert the distance from the top of the key to the bottom to the point location.
 * @param:  The index of the key.
 * @param:  The distance from the top of the key to the bottom.
 * @return: The point location.
 *
 *          The point location is calculated by the following formula.
 *          point = (distance * k) + b
 *          where b is the y-intercept of the line and k is the slope of the line.
 *          The line is defined by the key's attribute, such as the top and bottom values.
 */
uint16_t ksta_dst2pt(uint8_t ki, int16_t dst)
{
    return (uint16_t)((ksta[ki].k / (float)(ksta[ki].type_para->t2h_dst - dst)) + ksta[ki].b);
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline int16_t ksta_update_dst(uint8_t ki, uint16_t val)
{
    return ksta_pt2dst(ki, val);
}

/**
 * @brief:  Updete point location
 * @param:  None
 * @return: None
 */
static __inline uint16_t ksta_update_pt(uint8_t ki, int16_t pdst)
{
    return ksta_dst2pt(ki, pdst);
}

/**
 * @brief:  Update the state of the key.
 * @param:  The index of the key.
 * @param:  The new value of the key.
 * @return: None
 *
 *          Update the state of the key by updating the previous value, current value,
 *          value change, move direction, trigger event, enter event, action event,
 *          and position event.
 */
void ksta_updete(uint8_t ki, uint16_t val)
{
    ksta[ki].valp  = ksta[ki].valn;
    ksta[ki].valcp = ksta[ki].valc;

    ksta[ki].valn = val;
    ksta[ki].valc = ksta[ki].valn - ksta[ki].valp;

    ksta[ki].evt_md  = ksta_md_detect(ki);
    ksta[ki].evt_trg = ksta_trg_detect(ki);
    ksta[ki].evt_ent = ksta_ent_detect(ki);
    ksta[ki].evt_act = ksta_act_detect(ki);
    ksta[ki].dst     = ksta_update_dst(ki, ksta[ki].valn);
    ksta[ki].evt_pos = ksta_pos_detect(ki);
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
ksta_t *ksta_init(void)
{
    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        memset(&ksta[ki], 0, sizeof(ksta[ki]));
        ksta[ki].type_para = (ktp_para_t *)&g_ktp_para[g_kcfg.attr[ki].type];

        ksta_update_factor(ki);
        ksta[ki].pp = ksta_update_pt(ki, g_kcfg.attr[ki].nom.p_dst);
        ksta[ki].rp = ksta_update_pt(ki, g_kcfg.attr[ki].nom.r_dst);
    }
    return ksta;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void ksta_deinit(void)
{
}
