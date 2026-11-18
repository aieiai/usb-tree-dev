/**
 * @Author: wj
 * @Brief:  key report processing.
 * @Date:   2024/07/02
 */
#ifndef _KEY_REPORT_H
#define _KEY_REPORT_H
#include "utils.h"


typedef bool (*rpt_func_t)(uint8_t kc);

typedef enum {
    RPT_KB = 0,
    RPT_MS,
    RPT_SYS,
    RPT_USER,
    RPT_MAX,
} rpt_op_t;

typedef struct _rpt_kb_6kro {
    union {
        uint8_t dat;
        struct {
            uint8_t ctl_l : 1;
            uint8_t sft_l : 1;
            uint8_t alt_l : 1;
            uint8_t gui_l : 1;
            uint8_t ctl_r : 1;
            uint8_t sft_r : 1;
            uint8_t alt_r : 1;
            uint8_t gui_r : 1;
        } __attribute__((packed)) bit;
    } __attribute__((packed)) modify;
    uint8_t rsv;
    uint8_t keys[6];
} __attribute__((aligned(4))) krpt_kb6_t;

typedef union _rpt_kb_nkro {
    uint8_t keys[28];
    struct {
        uint8_t k1[21];
        uint8_t k2[7];
    } __attribute__((packed)) bit;
} __attribute__((aligned(4))) krpt_kbn_t;


typedef struct _rpt_mouse {
    uint8_t tmp;
} __attribute__((aligned(4))) krpt_ms_t;

typedef struct _rpt_sys {
    uint8_t tmp;
} __attribute__((aligned(4))) krpt_sys_t;

typedef struct _rpt_user {
    uint8_t tmp;
} __attribute__((aligned(4))) krpt_user_t;

typedef struct _key_report {
    uint8_t len;
    rpt_func_t fc_add;
    rpt_func_t fc_del;
    rpt_func_t fc_get;
} __attribute__((aligned(4))) krpt_t;


void krpt_deinit(void);
krpt_t *krpt_init(void);
void krpt_handle(void);


#endif /* _KEY_REPORT_H */
