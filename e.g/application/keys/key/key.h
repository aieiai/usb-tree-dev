/**
 * @Author: wj
 * @Brief:  key .
 * @Date:   2024/07/02
 */
#ifndef _KEY_H_
#define _KEY_H_
#include "utils.h"


typedef enum _key_operate_type {
    KOP_TYPE_NOM = 0,
    KOP_TYPE_RT,
    KOP_TYPE_ADV,
} kop_type_t;

typedef enum _key_step_type {
    KSTEP_TYPE_IDLE = 0,
    KSTEP_TYPE_INIT,
    KSTEP_TYPE_SLEEP,
    KSTEP_TYPE_WAKEUP,
    KSTEP_TYPE_SAMPLE_BEGIN,
    KSTEP_TYPE_SAMPLING,
    KSTEP_TYPE_SAMPLE_END,
    KSTEP_TYPE_FILTER,
    KSTEP_TYPE_HANDLE,
    KSTEP_TYPE_CHECK,
    KSTEP_TYPE_CALIB,
    KSTEP_TYPE_CALIB_AUTO,
    KSTEP_TYPE_RPT_FIX,
    KSTEP_TYPE_RPT_GIVE_WAIT,
    KSTEP_TYPE_RPT_TAKE_DONE,
    KSTEP_TYPE_LAMP_SET,
    KSTEP_TYPE_LAMP_MODE_SWITCH,
    KSTEP_TYPE_PARA_RELOAD,
    KSTEP_TYPE_SAVE,
    KSTEP_TYPE_TESTING,
    KSTEP_TYPE_MAX,
} kstep_type_t;

typedef struct _key_step {
    bool fst;
    kstep_type_t now;
    kstep_type_t pre;
    kstep_type_t cmd;
} __attribute__((aligned(4))) kstep_t;


// typedef struct _monitor {
//     uint16_t tmp;
// } __attribute__((packed)) kmont_t;

// typedef struct _output {
//     uint16_t lock : 1;
//     uint16_t rsv  : 15;
//     uint16_t code;
// } __attribute__((aligned(4))) kout_t;

typedef struct _key {
    // kstep_t *step;
    // ksta_t *sta;
    // kcal_t *cal;
    // krpt_t *rpt;
    // krt_t *rt;

    // kdbg_t *dbg;
    // kmont_t *mon;
    // kout_t  *out;
    uint8_t *tmp;
} __attribute__((aligned(4))) key_t;

extern key_t key;


typedef struct {
    float sampleTime;
    float sampleCycle;
    float keyHandleTime;
} __attribute__((aligned(4))) tu_t;

void key_main(void);


#endif /* _KEY_H_ */
