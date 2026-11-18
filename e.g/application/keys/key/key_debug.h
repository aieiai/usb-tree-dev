/**
 * @Author: wj
 * @Brief:  key debug.
 * @Date:   2024/07/02
 */
#ifndef _KEY_DEBUG_H
#define _KEY_DEBUG_H
#include "utils.h"

typedef struct _key_debug {
    bool clr;
    uint16_t val;
    uint16_t min;
    uint16_t max;
    uint16_t dif;
    uint16_t dst;
    float mvolt;
    float mvolt_dif;
} __attribute__((packed)) kdbg_t;


kdbg_t *kdbg_init(void);
void kdbg_deinit(void);

void kdbg_handle(void);
void kdbg_clear(uint8_t ki);
void kdbg_clears(void);
void kdbg_clear_monitor(void);

#endif /* _KEY_DEBUG_H */
