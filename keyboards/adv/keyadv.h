/**
 * @Author: wj
 * @Brief:  key advanced.
 * @Date:   2024/07/02
 */

#ifndef KEY_ADVANCED_H
#define KEY_ADVANCED_H

#include <stdint.h>

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

typedef struct {
    uint32_t magic;
    uint32_t crc32;
    uint32_t version;
    uint8_t  name[32];

} __attribute__((aligned(4))) oba_t;

extern oba_t g_oba;

oba_t *kadv_init(void);
oba_t *kadv_deinit(void);

#endif /* KEY_ADVANCED_H */


