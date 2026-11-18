/**
 * @Author: wj
 * @Brief:  key real-time status.
 * @Date:   2024/07/02
 */
#ifndef _KEY_STA_H_
#define _KEY_STA_H_
#include "utils.h"

#include "key_cfg.h"

typedef enum _key_event_result {
    KSTA_EVT_RES_U2U = 0,
    KSTA_EVT_RES_D2D,
} ksta_evt_res_t;

typedef enum _key_event_enter {
    KSTA_EVT_ENT_U2U = 0,
    KSTA_EVT_ENT_D2D,
} ksta_evt_ent_t;

typedef enum _key_event_trigger {
    KSTA_EVT_TRG_U2U = 0,
    KSTA_EVT_TRG_D2U,
    KSTA_EVT_TRG_D2D,
    KSTA_EVT_TRG_U2D,
} ksta_evt_trg_t;

typedef enum _key_event_move_direction {
    KSTA_EVT_MD_S = 0,
    KSTA_EVT_MD_U,
    KSTA_EVT_MD_D,
} ksta_evt_md_t;

typedef enum _key_event_position {
    KSTA_EVT_POS_TOP = 0,
    KSTA_EVT_POS_MID,
    KSTA_EVT_POS_BOT,
} ksta_evt_pos_t;

typedef enum _key_event_action {
    KSTA_EVT_ACT_IDLE = 0,
    KSTA_EVT_ACT_UP,
    KSTA_EVT_ACT_SC,
    KSTA_EVT_ACT_SCL,
    KSTA_EVT_ACT_SCLTO,
    KSTA_EVT_ACT_DC,
    KSTA_EVT_ACT_DCL,
    KSTA_EVT_ACT_DCLTO,
} ksta_evt_act_t;

typedef enum _key_action_status {
    KACT_STA_IDLE = 0,
    KACT_STA_UP,
    KACT_STA_SC,
    KACT_STA_SCL,
    KACT_STA_SCN,
    KACT_STA_DC,
    KACT_STA_DCL,
} kact_sta_t;

typedef struct _key_status {
    ksta_evt_md_t evt_md;   /* ksta_evt_md_t */
    ksta_evt_ent_t evt_ent; /* ksta_evt_ent_t */
    ksta_evt_trg_t evt_trg; /* ksta_evt_trg_t */
    ksta_evt_pos_t evt_pos; /* ksta_evt_pos_t */
    ksta_evt_act_t evt_act; /* ksta_evt_act_t */
    kact_sta_t act_sta;     /* kact_sta_t */
    ksta_evt_res_t evt_res; /* ksta_evt_res_t */

    ktp_para_t *type_para;
    uint16_t act_tk;
    uint16_t valn; /* value */
    uint16_t valp; /* value pre */
    int16_t valc;  /* value change */
    int16_t valcp; /* value change pre */
    uint16_t pp;   /* press point */
    uint16_t rp;   /* release point */
    int16_t dst;
    float k;
    float b;
} __attribute__((aligned(4))) ksta_t;


void ksta_deinit(void);
ksta_t *ksta_init(void);
int16_t ksta_pt2dst(uint8_t ki, uint16_t pt);
uint16_t ksta_dst2pt(uint8_t ki, int16_t dst);
void ksta_updete(uint8_t ki, uint16_t val);
void ksta_update_factor(uint8_t ki);

#endif /* _KEY_STA_H_ */
