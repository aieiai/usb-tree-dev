/**
 * @Author: wj
 * @Brief:  key advanced.
 * @Date:   2024/07/02
 */

#ifndef _KEY_ADVANCED_H_
#define _KEY_ADVANCED_H_
#include "utils.h"

typedef void *(*adv_func_t)(void *user);

typedef enum _key_advanced_type {
    KADV_TYPE_NONE = 0,
    KADV_TYPE_SHORTCUT,
    KADV_TYPE_REPLACE,
    KADV_TYPE_MACRO,
    KADV_TYPE_MT,
    KADV_TYPE_TGL,
    KADV_TYPE_DKS,
    KADV_TYPE_MAX,
} kadv_type_t;

typedef struct _key_advanced {
    uint16_t tmp;
} __attribute__((aligned(4))) kadv_t;



kadv_t *kadv_init(void);
kadv_t *kadv_deinit(void);

#endif /* _KEY_ADVANCED_H_ */


