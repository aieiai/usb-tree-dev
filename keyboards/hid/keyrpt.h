#ifndef KEY_RPT_H
#define KEY_RPT_H

#include <stdint.h>
#include <stdbool.h>

enum {
    KRPT_ID_NKRO = 0x01,
    KRPT_ID_CSM  = 0x02,
    KRPT_ID_SYS  = 0x03,
    KRPT_ID_6KRO = 0x04,
    KRPT_ID_MS   = 0x05,
    KRPT_ID_MAX
};

typedef union {
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
} __attribute__((packed)) keyrpt_mod_t;

typedef struct {
    uint8_t    id;
    keyrpt_mod_t modify;
    // union __attribute__((packed))  {
    //     uint8_t dat;
    //     struct {
    //         uint8_t ctl_l : 1;
    //         uint8_t sft_l : 1;
    //         uint8_t alt_l : 1;
    //         uint8_t gui_l : 1;
    //         uint8_t ctl_r : 1;
    //         uint8_t sft_r : 1;
    //         uint8_t alt_r : 1;
    //         uint8_t gui_r : 1;
    //     } bit;
    // } modify;
    uint8_t    rsv;
    uint8_t    keys[6];
} __attribute__((packed)) keyrpt_6kro_t;

typedef struct {
    uint8_t    id;
    keyrpt_mod_t modify;
    uint8_t    rsv;
    uint8_t    keys[16];
} __attribute__((packed)) keyrpt_nkro_t;

typedef struct {
    uint8_t  id;
    uint16_t usage;
} __attribute__((packed)) keyrpt_csm_t;

typedef struct {
    uint8_t id;
    uint8_t action;
} __attribute__((packed)) keyrpt_sys_t;

typedef struct {
    uint8_t id;
    uint8_t buttons;
    int16_t x;
    int16_t y;
    int16_t wheel;
    int16_t pan;
} __attribute__((packed)) keyrpt_ms_t;


typedef struct {
    keyrpt_6kro_t kro;
    keyrpt_nkro_t nkro;
    keyrpt_csm_t  csm;
    keyrpt_sys_t  sys;
    keyrpt_ms_t   ms;
    
    uint8_t  dirty_mask; /* 按 KRPT_BIT(id) 标记待发送的报告 */
} keyrpt_ctx_t;

extern keyrpt_ctx_t g_keyrpt_ctx;

void keyrpt_6kro_add(uint8_t kc);
void keyrpt_6kro_del(uint8_t kc);
bool keyrpt_6kro_get(uint8_t kc);

void keyrpt_nkro_add(uint8_t kc);
void keyrpt_nkro_del(uint8_t kc);
bool keyrpt_nkro_get(uint8_t kc);

void keyrpt_csm_add(uint16_t cc);
void keyrpt_csm_del(uint16_t cc);
bool keyrpt_csm_get(uint16_t cc);

void keyrpt_sys_add(uint8_t sc);
void keyrpt_sys_del(uint8_t sc);
bool keyrpt_sys_get(uint8_t sc);

void keyrpt_ms_add_btn(uint8_t btn);
void keyrpt_ms_del_btn(uint8_t btn);
bool keyrpt_ms_get_btn(uint8_t btn);
void keyrpt_ms_clr(void);
void keyrpt_ms_set_move(int16_t dx, int16_t dy);
void keyrpt_ms_set_wheel(int16_t dw);

/* 获取指定报告 ID 的原始数据（首字节为报告 ID），用于通过 USB 端点发送。
 * 返回报告长度，buf 为 NULL 或返回 0 表示报告 ID 无效。 */
uint16_t keyrpt_get_raw(uint8_t id, const uint8_t **buf);



#endif  /* KEYRPT_H */
