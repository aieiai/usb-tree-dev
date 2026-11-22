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

    // ktp_para_t *type_para;
    uint16_t act_tk;
    uint16_t val;  /* value */
    uint16_t valp; /* value pre */
    int16_t  vald;  /* value delta */
    int16_t  valdp; /* value delta pre */
    uint16_t pv;   /* press value */
    uint16_t rv;   /* release value */
    int16_t  dst;
} __attribute__((aligned(4))) keyevt_t;

extern keyevt_t g_keyevt[KBD_KEY_MAX];



void keyevt_init(void);
void keyevt_updete(uint8_t ki, uint16_t val);




#endif  /* KEY_EVENT_H */

