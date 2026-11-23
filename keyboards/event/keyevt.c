/**
 * @Author: wj
 * @Brief:  key real-time event.
 * @Date:   2024/07/02
 */
#include "keyevt.h"
#include "keyaxis.h"
#include "keyprop.h"
#include "keymagnet.h"

#include <string.h>

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
    uint32_t cnt;
    kact_sta_ty sta;
} msg_t;

static msg_t msg[KBD_KEY_MAX];
keyevt_t g_keyevt[KBD_KEY_MAX] = {0};



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
    keyevt_t *p_evt = &g_keyevt[ki];
    keyattr_t *p_attr = &g_obkp.attr[ki];
    msg_t *p_msg = &msg[ki];

    switch (p_msg->sta) {
        case KACT_STA_IDLE:
            if (EVT_IS_U(p_evt->trg)) {
                p_evt->act = KEVT_ACT_IDLE; /* idle */
            }
            else {
                p_msg->sta = KACT_STA_SC;
                p_msg->cnt  = 0;
            }
        break;

        case KACT_STA_SC:
            if (EVT_IS_U(p_evt->trg)) {
                p_msg->cnt  = 0;
                p_msg->sta = KACT_STA_SCN;
            }
            else {
                if (p_msg->cnt >= MS2TK(p_attr->act.sclt)) {
                    p_msg->cnt  = 0;
                    p_msg->sta = KACT_STA_SCL;
                }
                else {
                    p_msg->cnt++;
                }
            }
        break;

        case KACT_STA_SCN:
            if (EVT_IS_D(p_evt->trg)) {
                p_msg->cnt  = 0;
                p_msg->sta = KACT_STA_DC;
            }
            else {
                if (p_msg->cnt >= MS2TK(p_attr->act.dclt)) {
                    p_msg->cnt  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                    p_evt->act = KEVT_ACT_SC; /* click */
                }
                else {
                    p_msg->cnt++;
                }
            }
        break;

        case KACT_STA_DC:
            if (EVT_IS_U(p_evt->trg)) {
                p_msg->cnt  = 0;
                p_msg->sta = KACT_STA_IDLE;
                p_evt->act = KEVT_ACT_DC; /* double click */
            }
            else {
                if (p_msg->cnt >= MS2TK(p_attr->act.dclt)) {
                    p_msg->cnt  = 0;
                    p_msg->sta = KACT_STA_DCL;
                }
                else {
                    p_msg->cnt++;
                }
            }
        break;

        case KACT_STA_SCL:
            if (EVT_IS_D(p_evt->trg)) {
                if (p_msg->cnt >= MS2TK(p_attr->act.sclto)) {
                    p_msg->cnt  = 0;
                    p_msg->sta = KACT_STA_UP;
                    p_evt->act = KEVT_ACT_SCLTO; /* click long timeout */
                }
                else {
                    p_msg->cnt++;
                }
            }
            else {
                if (p_msg->cnt >= MS2TK(p_attr->act.sclto)) {
                    p_msg->cnt  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                }
                else {
                    p_msg->cnt  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                    p_evt->act = KEVT_ACT_SCL; /* click long */
                }
            }
        break;

        case KACT_STA_DCL:
            if (EVT_IS_D(p_evt->trg)) {
                if (p_msg->cnt >= MS2TK(p_attr->act.dclto)) {
                    p_msg->cnt  = 0;
                    p_msg->sta = KACT_STA_UP;
                    p_evt->act = KEVT_ACT_DCLTO; /* double click long timeout */
                }
                else {
                    p_msg->cnt++;
                }
            }
            else {
                if (p_msg->cnt >= MS2TK(p_attr->act.dclto)) {
                    p_msg->cnt  = 0;
                    p_msg->sta = KACT_STA_IDLE;
                }
                else {
                    p_msg->cnt  = 0;
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
    keyevt_t *p_evt = &g_keyevt[ki];
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
    keyevt_t *p_evt = &g_keyevt[ki];
    return (EVT_IS_U(p_evt->trg) ? ((p_evt->val <= p_evt->pv) ? KEVT_TRG_U2D : KEVT_TRG_U2U)
                                 : ((p_evt->val >= p_evt->rv) ? KEVT_TRG_D2U : KEVT_TRG_D2D));
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
static __inline kevt_pos_ty _pos_detect(uint8_t ki)
{
    keyevt_t *p_evt = &g_keyevt[ki];
    const keyaxis_para_t *p_axis = p_evt->axis_para;
    return ((p_evt->dst <= 50.0f) ? KEVT_POS_TOP : (p_evt->dst > p_axis->t2b_dst) ? KEVT_POS_BOT : KEVT_POS_MID);
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
void keyevt_updete(uint8_t ki, uint16_t val)
{
    g_keyevt[ki].valp  = g_keyevt[ki].val;
    g_keyevt[ki].valdp = g_keyevt[ki].vald;
    g_keyevt[ki].val   = val;
    g_keyevt[ki].vald  = g_keyevt[ki].val - g_keyevt[ki].valp;

    g_keyevt[ki].dst = keymag_pt2du(ki, g_keyevt[ki].val);
    g_keyevt[ki].md  = _md_detect(ki);
    g_keyevt[ki].trg = _trg_detect(ki);
    g_keyevt[ki].act = _act_detect(ki);
    // g_keyevt[ki].pos = _pos_detect(ki);
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void keyevt_init(void)
{
    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        keyevt_t *p_evt = &g_keyevt[ki];
        keyattr_t *p_attr = &g_obkp.attr[ki];
        
        memset(p_evt, 0, sizeof(keyevt_t));
        p_evt->pv = keymag_du2pt(ki, p_attr->nom.dst_p);
        p_evt->rv = keymag_du2pt(ki, p_attr->nom.dst_r);
        p_evt->axis_para = &g_keyaxis_para[p_attr->axis.ty];
    }
}

