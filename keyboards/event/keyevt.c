/**
 * @Author: wj
 * @Brief:  key real-time event.
 * @Date:   2024/07/02
 */
#include "keyevt.h"
#include "kbd_config.h"

// key action status
typedef enum {
    KACT_STA_IDLE = 0,
    KACT_STA_UP,
    KACT_STA_SC,
    KACT_STA_SCL,
    KACT_STA_SCN,
    KACT_STA_DC,
    KACT_STA_DCL,
} kact_sta_ty;

typedef struct {
    kact_sta_ty sta;
    uint32_t act_tk;
    uint32_t t2b_dst;
} kmsg_t;

typedef struct {
    uint32_t sclt;
    uint32_t sclto;
    uint32_t dclt;
    uint32_t dclto;
} ktk_t;

kevt_t g_kevt[KBD_KEY_MAX];
static kmsg_t kmsg[KBD_KEY_MAX];
static ktk_t ktk[KBD_KEY_MAX];



/**
 * @brief:  Detect the action event of the key.
 * @param:  The index of the key.
 * @return: The action event of the key.
 *
 *          Detect the action event of the key by the enter event and action state.
 *          The action event can be one of the following:
 *            - KEVT_ACT_IDLE: Idle.
 *            - KEVT_ACT_UP: Up.
 *            - KEVT_ACT_SC: Click.
 *            - KEVT_ACT_SCL: Click long.
 *            - KEVT_ACT_SCLTO: Click long timeout.
 *            - KEVT_ACT_DC: Double click.
 *            - KEVT_ACT_DCL: Double click long.
 *            - KEVT_ACT_DCLTO: Double click long timeout.
 */
static __inline kevt_act_ty _act_detect(uint8_t ki)
{
    kevt_t *p_evt = &g_kevt[ki];
    kmsg_t *p_msg = &kmsg[ki];
    ktk_t *p_tk   = &ktk[ki];

    switch (p_msg->sta) {
        case KACT_STA_IDLE:
            if (EVT_IS_U(p_evt->trg)) {
                p_evt->act = KEVT_ACT_IDLE; /* idle */
                break;
            }
            else {
                p_msg->sta = KACT_STA_SC;
                p_msg->act_tk  = 0;
            }
        // break;

        case KACT_STA_SC:
            if (EVT_IS_U(p_evt->trg)) {
                p_msg->act_tk  = 0;
                p_msg->sta = KACT_STA_SCN;
            }
            else {
                if (p_msg->act_tk >= p_tk->sclt) {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_SCL;
                }
                else {
                    p_msg->act_tk++;
                }
            }
        break;

        case KACT_STA_SCN:
            if (EVT_IS_D(p_evt->trg)) {
                p_msg->act_tk  = 0;
                p_msg->sta = KACT_STA_DC;
            }
            else {
                if (p_msg->act_tk >= p_tk->dclt) {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                    p_evt->act = KEVT_ACT_SC; /* click */
                }
                else {
                    p_msg->act_tk++;
                }
            }
        break;

        case KACT_STA_DC:
            if (EVT_IS_U(p_evt->trg)) {
                p_msg->act_tk  = 0;
                p_msg->sta = KACT_STA_IDLE;
                p_evt->act = KEVT_ACT_DC; /* double click */
            }
            else {
                if (p_msg->act_tk >= p_tk->dclt) {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_DCL;
                }
                else {
                    p_msg->act_tk++;
                }
            }
        break;

        case KACT_STA_SCL:
            if (EVT_IS_D(p_evt->trg)) {
                if (p_msg->act_tk >= p_tk->sclto) {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_UP;
                    p_evt->act = KEVT_ACT_SCLTO; /* click long timeout */
                }
                else {
                    p_msg->act_tk++;
                }
            }
            else {
                if (p_msg->act_tk >= p_tk->sclto) {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                }
                else {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                    p_evt->act = KEVT_ACT_SCL; /* click long */
                }
            }
        break;

        case KACT_STA_DCL:
            if (EVT_IS_D(p_evt->trg)) {
                if (p_msg->act_tk >= p_tk->dclto) {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_UP;
                    p_evt->act = KEVT_ACT_DCLTO; /* double click long timeout */
                }
                else {
                    p_msg->act_tk++;
                }
            }
            else {
                if (p_msg->act_tk >= p_tk->dclto) {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                }
                else {
                    p_msg->act_tk  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                    p_evt->act = KEVT_ACT_DCL; /* double click long */
                }
            }
        break;

        case KACT_STA_UP:
            if (EVT_IS_U(p_evt->trg)) {
                p_msg->sta = KACT_STA_IDLE;
                p_evt->act = KEVT_ACT_UP; /* up */
            }
        break;
    }
    return p_evt->act;
}

/**
 * @brief:  Detect the move direction of the key.
 * @param:  The index of the key.
 * @return: The move direction of the key.
 *
 *          Detect the move direction of the key by comparing the current value
 *          with the previous value. The move direction can be one of the following:
 *            - KEVT_MD_U: Move up.
 *            - KEVT_MD_D: Move down.
 *            - KEVT_MD_S: Stay still.
 */
static __inline kevt_md_ty _md_detect(uint8_t ki)
{
    kevt_t *p_evt = &g_kevt[ki];
    return ((p_evt->vald > 0) ? KEVT_MD_U : (p_evt->vald < 0) ? KEVT_MD_D : KEVT_MD_S);
}

/**
 * @brief:  Detect the trigger event of the key.
 * @param:  The index of the key.
 * @return: The trigger event of the key.
 *
 *          Detect the trigger event of the key by comparing the current value
 *          with the previous value and threshold. The trigger event can be one
 *          of the following:
 *            - KEVT_TRG_U2U: From up to up.
 *            - KEVT_TRG_U2D: From up to down.
 *            - KEVT_TRG_D2D: From down to down.
 *            - KEVT_TRG_D2U: From down to up.
 */
static __inline kevt_trg_ty _trg_detect(uint8_t ki)
{
    kevt_t *p_evt = &g_kevt[ki];
    return (EVT_IS_U(p_evt->trg) ? ((p_evt->val <= p_evt->pv) ? KEVT_TRG_U2D : KEVT_TRG_U2U)
                                 : ((p_evt->val >= p_evt->rv) ? KEVT_TRG_D2U : KEVT_TRG_D2D));
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
// static __inline kevt_ent_ty kevt_ent_detect(uint8_t ki)
// {
//     kevt_t *p_evt = &g_kevt[ki];
//     return ((EVT_IS_D(p_evt->trg) ? KEVT_ENT_U2U : KEVT_ENT_D2D));
// }

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
static __inline kevt_pos_ty _pos_detect(uint8_t ki)
{
    kevt_t *p_evt = &g_kevt[ki];
    kmsg_t *p_msg = &kmsg[ki];
    return ((p_evt->dst <= 50) ? KEVT_POS_TOP : (p_evt->dst >= p_msg->t2b_dst) ? KEVT_POS_BOT : KEVT_POS_MID);
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
    float t2hdm = g_kevt[ki].type_para->t2h_dst / 1000.0f;
    float b2hdm = g_kevt[ki].type_para->b2h_dst / 1000.0f;
    float t2bv  = topv - botv;
    float k     = (HAL_ADC_VEF / (float)((0x01 << HAL_ADC_BIT) - 1.0f) * t2bv) / ((1.0f / t2hdm) - (1.0f / b2hdm));
    float b     = (HAL_ADC_VEF / (float)((0x01 << HAL_ADC_BIT) - 1.0f) * topv) - (k / t2hdm);
    g_kevt[ki].k  = (1000.0f * 1000.0f / (HAL_ADC_VEF * 1000.0f) * (float)((0x01 << HAL_ADC_BIT) - 1.0f)) * k;
    g_kevt[ki].b  = (1000.0f * (float)((0x01 << HAL_ADC_BIT) - 1.0f) / (HAL_ADC_VEF * 1000.0f)) * b;
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
    return (uint16_t)((g_kevt[ki].k / (float)(g_kevt[ki].type_para->t2h_dst - dst)) + g_kevt[ki].b);
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline int16_t _update_dst(uint8_t ki, uint16_t val)
{
    return ksta_pt2dst(ki, val);
}

/**
 * @brief:  Updete point location
 * @param:  None
 * @return: None
 */
static __inline uint16_t _update_pt(uint8_t ki, int16_t pdst)
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
void kevt_updete(uint8_t ki, uint16_t val)
{
    g_kevt[ki].valp  = g_kevt[ki].val;
    g_kevt[ki].valdp = g_kevt[ki].vald;
    g_kevt[ki].val   = val;
    g_kevt[ki].vald  = g_kevt[ki].val - g_kevt[ki].valp;

    g_kevt[ki].md  = kevt_md_detect(ki);
    g_kevt[ki].trg = kevt_trg_detect(ki);
    g_kevt[ki].act = kevt_act_detect(ki);
    g_kevt[ki].dst = kevt_update_dst(ki, g_kevt[ki].val);
    g_kevt[ki].pos = kevt_pos_detect(ki);
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
kevt_t *kevt_init(void)
{
    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        memset(&g_kevt[ki], 0, sizeof(g_kevt[ki]));
        g_kevt[ki].type_para = (ktp_para_t *)&g_ktp_para[g_kcfg.attr[ki].type];

        ksta_update_factor(ki);
        g_kevt[ki].pv = ksta_update_pt(ki, g_kcfg.attr[ki].nom.p_dst);
        g_kevt[ki].rv = ksta_update_pt(ki, g_kcfg.attr[ki].nom.r_dst);
    }
    return g_kevt;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void ksta_deinit(void)
{
}
