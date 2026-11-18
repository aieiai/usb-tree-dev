/**
 * @Author: wj
 * @Brief:  key calib processing.
 * @Date:   2024/07/02
 */
#ifndef _KEY_CALIB_H
#define _KEY_CALIB_H
#include "utils.h"

typedef enum _key_calibration_step_type {
    KCAL_STEP_IDLE = 0,
    KCAL_STEP_RESTART,
    KCAL_STEP_START,
    KCAL_STEP_ABORT,
    KCAL_STEP_CALING,
    KCAL_STEP_DONE,
    KCAL_STEP_EXIT,
} kcal_step_type_t;

typedef enum _key_calibration_result_type {
    KCAL_RES_EXIT = 0,
    KCAL_RES_EXIT_DONE,
    KCAL_RES_EXIT_RELOAD,
    KCAL_RES_KEEP,
} kcal_res_type_t;

typedef union _key_limit_value {
    uint16_t buf[2];
    struct {
        uint16_t top;
        uint16_t bot;
    };
    struct {
        uint16_t max;
        uint16_t min;
    };
} __attribute__((aligned(4))) kcal_lmt_t;

typedef struct _key_calibration_step {
    bool fst;
    kcal_step_type_t now;
    kcal_step_type_t pre;
    kcal_step_type_t cmd;
} __attribute__((aligned(4))) kcal_step_t;

typedef struct _key_calibration_result {
    bool save;
    kcal_res_type_t res;
} __attribute__((aligned(4))) kcal_res_t;

typedef struct _key_calibration_info {
    uint16_t dbit;
    uint8_t dcnt;
    uint8_t done;
} __attribute__((packed)) kcal_info_t;

typedef struct _key_calibration_monitor {
    kcal_lmt_t val;
    uint8_t pass;
} __attribute__((aligned(4))) kcal_mnt_t;

typedef struct _key_calibration {
    kcal_step_t *step;
    kcal_res_t *res;
    kcal_info_t *info;
    kcal_mnt_t *mnt;
} __attribute__((aligned(4))) kcal_t;


void kcalib_deinit(void);
kcal_t *kcalib_init(void);
kcal_res_type_t kcalib_process(void);

#endif /* _KEY_CALIB_H */
