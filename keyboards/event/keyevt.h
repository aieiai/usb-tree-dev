/**
 * @Author: wj
 * @Brief:  key real-time event.
 * @Date:   2024/07/02
 */
#ifndef KEY_EVENT_H
#define KEY_EVENT_H

#include <stdint.h>


#ifndef KBD_KEY_NUM
#define KBD_KEY_NUM 42
#endif

#ifndef CK_KI
#define CK_KI(ki) ((ki) < KBD_KEY_NUM)
#endif

#ifndef CKK_IS_U
#define CKK_IS_U(trg) ((trg) < KEVT_TRG_D2D)
#endif

#ifndef CKK_IS_D
#define CKK_IS_D(trg) ((trg) > KEVT_TRG_D2U)
#endif

#ifndef CKK_IS_FU
#define CKK_IS_FU(trg) ((trg) == KEVT_TRG_D2U)
#endif

#ifndef CKK_IS_FD
#define CKK_IS_FD(trg) ((trg) == KEVT_TRG_U2D)
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
    int16_t vald;  /* value delta */
    int16_t valdp; /* value delta pre */
    uint16_t pv;   /* press value */
    uint16_t rv;   /* release value */
    int16_t dst;
    float k;
    float b;
} __attribute__((aligned(4))) kevt_t;


void kevt_deinit(void);
kevt_t *kevt_init(void);
int16_t kevt_pt2dst(uint8_t ki, uint16_t pt);
uint16_t kevt_dst2pt(uint8_t ki, int16_t dst);
void kevt_updete(uint8_t ki, uint16_t val);
void kevt_update_factor(uint8_t ki);

#endif  /* KEY_EVENT_H */

