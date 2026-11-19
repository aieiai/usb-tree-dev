/**
 * @Author: wj
 * @Brief:  key rapid trigger.
 * @Date:   2024/07/02
 */
#ifndef KEY_RAPID_TRIGGER_H
#define KEY_RAPID_TRIGGER_H
#include "stdint.h"
#include "stdbool.h"

typedef enum _key_rt_event_enter {
    KRT_EVT_ENT_U2U = 0,
    KRT_EVT_ENT_D2D,
} krt_evt_ent_t;

typedef enum _key_rt_event_trigger {
    KRT_EVT_TRG_U2U = 0,
    KRT_EVT_TRG_D2U,
    KRT_EVT_TRG_D2D,
    KRT_EVT_TRG_U2D,
} krt_evt_trg_t;

typedef enum _key_rt_puls_type {
    KRT_PLS_MD_RDT = 0,
    KRT_PLS_MD_MAX,
} krt_pls_type_t;


typedef struct _key_rt {
    bool rst;
    krt_evt_ent_t evt_ent;   /* ksta_evt_ent_t */
    krt_evt_trg_t evt_trg;   /* ksta_evt_trg_t */
    int16_t pd_dst;      /* press depth dist */
    int16_t rd_dst;      /* raise depth dist */
    int16_t pnm_dst;     /* press need move dist */
    int16_t rnm_dst;     /* raise need move dist */
} __attribute__((aligned(4))) krt_t;



void krt_deinit(void);
krt_t *krt_init(void);
void krt_process(uint8_t ki);

void krt_sta_rst(uint8_t ki);
void krt_sta_rst_force(uint8_t ki);




#endif      /* _KEY_RAPID_TRIGGER_H_ */


