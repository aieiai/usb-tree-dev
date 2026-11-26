/**
 * @Author: wj
 * @Brief:  key real-time event.
 * @Date:   2024/07/02
 */
#ifndef KEY_EVENT_H
#define KEY_EVENT_H

#include <stdint.h>
#include "kbd_config.h"



#ifndef EVT_IS_U
#define EVT_IS_U(trg) ((trg) < KEVT_TRG_D2D)
#endif

#ifndef EVT_IS_D
#define EVT_IS_D(trg) ((trg) > KEVT_TRG_D2U)
#endif

#ifndef EVT_IS_FU
#define EVT_IS_FU(trg) ((trg) == KEVT_TRG_D2U)
#endif

#ifndef EVT_IS_FD
#define EVT_IS_FD(trg) ((trg) == KEVT_TRG_U2D)
#endif


// key event trigger
typedef enum {
    KEVT_TRG_U2U = 0,
    KEVT_TRG_D2U,
    KEVT_TRG_D2D,
    KEVT_TRG_U2D,
} kevt_trg_ty;

// key event move direction
typedef enum {
    KEVT_MD_S = 0,
    KEVT_MD_U,
    KEVT_MD_D,
} kevt_md_ty;

// key event position
typedef enum {
    KEVT_POS_TOP = 0,
    KEVT_POS_MID,
    KEVT_POS_BOT,
} kevt_pos_ty;

// key event action
typedef enum {
    KEVT_ACT_IDLE = 0,
    KEVT_ACT_UP,
    KEVT_ACT_SC,
    KEVT_ACT_SCL,
    KEVT_ACT_SCLTO,
    KEVT_ACT_DC,
    KEVT_ACT_DCL,
    KEVT_ACT_DCLTO,
} kevt_act_ty;

// key status
typedef struct {
    kevt_trg_ty trg;
    kevt_md_ty  md;
    kevt_pos_ty pos;
    kevt_act_ty act;

    kevt_act_ty sta;
    uint32_t cnt;

    const void *axis_para;
    uint16_t act_tk;
    uint16_t val;  /* value */
    uint16_t valp; /* value pre */
    int16_t  vald;  /* value delta */
    int16_t  valdp; /* value delta pre */
    uint16_t pv;   /* press value */
    uint16_t rv;   /* release value */
    float  dst;
} __attribute__((aligned(4))) keyevt_t;

extern keyevt_t g_keyevt[KBD_KEY_MAX];

void keyevt_init(void);
void keyevt_updete(uint8_t ki, uint16_t val);
void keyevt_reload(void);


/**
 * @brief:  Detect the move direction of the key.
 * @param:  The index of the key.
 * @return: The move direction of the key (kevt_md_ty).
 *
 *          Detect the move direction of the key by comparing the current value
 *          with the previous value. The move direction can be one of the following:
 *            - KEVT_MD_U: Move up.
 *            - KEVT_MD_D: Move down.
 *            - KEVT_MD_S: Stay still.
 */
#define _keyevt_md_detect(ki)               (p_evt->md = ((p_evt->vald > 0) ? KEVT_MD_U : (p_evt->vald < 0) ? KEVT_MD_D : KEVT_MD_S))

/**
 * @brief:  Detect the position event of the key.
 * @param:  The index of the key.
 * @return: The position event of the key (kevt_pos_ty).
 *
 *          Detect the position event of the key by comparing the current value
 *          with the threshold. The position event can be one of the following:
 *            - KSTA_EVT_POS_TOP: The key is released and the position is at the top.
 *            - KSTA_EVT_POS_BOT: The key is pressed and the position is at the bottom.
 *            - KSTA_EVT_POS_MID: The key is pressed and the position is between the top and the bottom.
 */
#define _keyevt_pos_detect(p_evt, p_axis)   (p_evt->pos = ((p_evt->dst <= 50.0f) ? KEVT_POS_TOP : (p_evt->dst > p_axis->t2b_dst) ? KEVT_POS_BOT : KEVT_POS_MID))

/**
 * @brief:  Detect the trigger event of the key.
 * @param:  The index of the key.
 * @return: The trigger event of the key (kevt_trg_ty).
 *
 *          Detect the trigger event of the key by comparing the current value
 *          with the previous value and threshold. The trigger event can be one
 *          of the following:
 *            - KEVT_TRG_U2U: From up to up.
 *            - KEVT_TRG_U2D: From up to down.
 *            - KEVT_TRG_D2D: From down to down.
 *            - KEVT_TRG_D2U: From down to up.
 */
#define _keyevt_trg_detect(p_evt)           (p_evt->trg = (EVT_IS_U(p_evt->trg) ? ((p_evt->val <= p_evt->pv) ? KEVT_TRG_U2D : KEVT_TRG_U2U) : ((p_evt->val >= p_evt->rv) ? KEVT_TRG_D2U : KEVT_TRG_D2D)))


#define _keyevt_act_detect(p_evt, p_attr, ki) {                                     \
    switch (p_evt->sta) {                                                           \
        case KEVT_ACT_IDLE:                                                         \
            if (EVT_IS_U(p_evt->trg)) {                                             \
                p_evt->act = KEVT_ACT_IDLE;                                         \
            }                                                                       \
            else {                                                                  \
                p_evt->sta = KEVT_ACT_SC;                                           \
                p_evt->cnt = 0;                                                     \
            }                                                                       \
        break;                                                                      \
        case KEVT_ACT_SC:                                                           \
            if (EVT_IS_U(p_evt->trg)) {                                             \
                p_evt->cnt = 0;                                                     \
                p_evt->sta = KEVT_ACT_SCLTO; /* KEVT_ACT_SCN */                     \
            }                                                                       \
            else {                                                                  \
                if (p_evt->cnt >= MS2TK(p_attr->act.sclt)) {                        \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_SCL;                                      \
                }                                                                   \
                else {                                                              \
                    p_evt->cnt++;                                                   \
                }                                                                   \
            }                                                                       \
        break;                                                                      \
        case KEVT_ACT_SCLTO: /* KEVT_ACT_SCN */                                     \
            if (EVT_IS_D(p_evt->trg)) {                                             \
                p_evt->cnt = 0;                                                     \
                p_evt->sta = KEVT_ACT_DC;                                           \
            }                                                                       \
            else {                                                                  \
                if (p_evt->cnt >= MS2TK(p_attr->act.dclt)) {                        \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_IDLE;                                     \
                    p_evt->act = KEVT_ACT_SC; /* click */                           \
                }                                                                   \
                else {                                                              \
                    p_evt->cnt++;                                                   \
                }                                                                   \
            }                                                                       \
        break;                                                                      \
        case KEVT_ACT_DC:                                                           \
            if (EVT_IS_U(p_evt->trg)) {                                             \
                p_evt->cnt = 0;                                                     \
                p_evt->sta = KEVT_ACT_IDLE;                                         \
                p_evt->act = KEVT_ACT_DC; /* double click */                        \
            }                                                                       \
            else {                                                                  \
                if (p_evt->cnt >= MS2TK(p_attr->act.dclt)) {                        \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_DCL;                                      \
                }                                                                   \
                else {                                                              \
                    p_evt->cnt++;                                                   \
                }                                                                   \
            }                                                                       \
        break;                                                                      \
        case KEVT_ACT_SCL:                                                          \
            if (EVT_IS_D(p_evt->trg)) {                                             \
                if (p_evt->cnt >= MS2TK(p_attr->act.sclto)) {                       \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_UP;                                       \
                    p_evt->act = KEVT_ACT_SCLTO; /* click long timeout */           \
                }                                                                   \
                else {                                                              \
                    p_evt->cnt++;                                                   \
                }                                                                   \
            }                                                                       \
            else {                                                                  \
                if (p_evt->cnt >= MS2TK(p_attr->act.sclto)) {                       \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_IDLE;                                     \
                }                                                                   \
                else {                                                              \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_IDLE;                                     \
                    p_evt->act = KEVT_ACT_SCL; /* click long */                     \
                }                                                                   \
            }                                                                       \
        break;                                                                      \
        case KEVT_ACT_DCL:                                                          \
            if (EVT_IS_D(p_evt->trg)) {                                             \
                if (p_evt->cnt >= MS2TK(p_attr->act.dclto)) {                       \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_UP;                                       \
                    p_evt->act = KEVT_ACT_DCLTO; /* double click long timeout */    \
                }                                                                   \
                else {                                                              \
                    p_evt->cnt++;                                                   \
                }                                                                   \
            }                                                                       \
            else {                                                                  \
                if (p_evt->cnt >= MS2TK(p_attr->act.dclto)) {                       \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_IDLE;                                     \
                }                                                                   \
                else {                                                              \
                    p_evt->cnt = 0;                                                 \
                    p_evt->sta = KEVT_ACT_IDLE;                                     \
                    p_evt->act = KEVT_ACT_DCL; /* double click long */              \
                }                                                                   \
            }                                                                       \
        break;                                                                      \
        case KEVT_ACT_UP:                                                           \
            if (EVT_IS_U(p_evt->trg)) {                                             \
                p_evt->sta = KEVT_ACT_IDLE;                                         \
                p_evt->act = KEVT_ACT_UP; /* up */                                  \
            }                                                                       \
        break;                                                                      \
        default:                                                                    \
            p_evt->sta = KEVT_ACT_IDLE;                                             \
            p_evt->act = KEVT_ACT_UP; /* up */                                      \
        break;                                                                      \
    }                                                                               \
}




#endif  /* KEY_EVENT_H */

