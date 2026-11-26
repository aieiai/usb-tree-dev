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
static inline kevt_act_ty _act_detect(uint8_t ki)
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
    keyevt_t *p_evt = &g_keyevt[ki];
    keymag_t *p_mag = &g_keymag[ki];
    const keyaxis_para_t *p_axis = p_evt->axis_para;

    p_evt->valp  = p_evt->val;
    p_evt->valdp = p_evt->vald;
    p_evt->val   = val;
    p_evt->vald  = p_evt->val - p_evt->valp;

    p_evt->dst = _keymag_pt2du(p_mag, p_axis, p_evt->val);
    p_evt->md  = _keyevt_md_detect(p_evt);
    p_evt->trg = _keyevt_trg_detect(p_evt);
    // p_evt->act = _act_detect(ki);
    p_evt->pos = _keyevt_pos_detect(p_evt, p_axis);
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

void keyevt_reload(void)
{
    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        keyevt_t *p_evt = &g_keyevt[ki];
        keyattr_t *p_attr = &g_obkp.attr[ki];
        
        p_evt->pv = keymag_du2pt(ki, p_attr->nom.dst_p);
        p_evt->rv = keymag_du2pt(ki, p_attr->nom.dst_r);
        p_evt->axis_para = &g_keyaxis_para[p_attr->axis.ty];
    }
}
