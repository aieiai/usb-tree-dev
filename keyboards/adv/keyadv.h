/**
 * @Author: wj
 * @Brief:  key advanced.
 * @Date:   2024/07/02
 */

#ifndef KEY_ADVANCED_H
#define KEY_ADVANCED_H
#include "utils.h"

typedef void *(*adv_func_t)(void *user);

typedef enum {
    KADV_TY_NONE = 0,
    KADV_TY_SHORTCUT,
    KADV_TY_REPLACE,
    KADV_TY_MACRO,
    KADV_TY_MT,
    KADV_TY_TGL,
    KADV_TY_DKS,
    KADV_TY_MAX,
} kadv_ty;

typedef struct _key_advanced {
    uint16_t tmp;
} __attribute__((aligned(4))) kadv_t;



kadv_t *kadv_init(void);
kadv_t *kadv_deinit(void);

#endif /* KEY_ADVANCED_H */


