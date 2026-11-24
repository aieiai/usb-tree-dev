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
} __attribute__((packed)) krpt_mod_t;

typedef struct {
    uint8_t    id;
    krpt_mod_t modify;
    uint8_t    rsv;
    uint8_t    keys[6];
} __attribute__((packed)) krpt_6kro_t;

typedef struct {
    uint8_t    id;
    krpt_mod_t modify;
    uint8_t    rsv;
    uint8_t    keys[16];
} __attribute__((packed)) krpt_nkro_t;

typedef struct {
    uint8_t  id;
    uint16_t usage;
} __attribute__((packed)) krpt_csm_t;

typedef struct {
    uint8_t id;
    uint8_t action;
} __attribute__((packed)) krpt_sys_t;

typedef struct {
    uint8_t id;
    uint8_t buttons;
    int16_t x;
    int16_t y;
    int16_t wheel;
    int16_t pan;
} __attribute__((packed)) krpt_ms_t;

void krpt_add_6kro(uint8_t kc);
void krpt_del_6kro(uint8_t kc);
bool krpt_get_6kro(uint8_t kc);

void krpt_add_nkro(uint8_t kc);
void krpt_del_nkro(uint8_t kc);
bool krpt_get_nkro(uint8_t kc);

void krpt_add_csm(uint16_t cc);
void krpt_del_csm(uint16_t cc);
bool krpt_get_csm(uint16_t cc);

void krpt_add_sys(uint8_t sc);
void krpt_del_sys(uint8_t sc);
bool krpt_get_sys(uint8_t sc);

void krpt_add_ms(uint8_t btn);
void krpt_del_ms(uint8_t btn);
bool krpt_get_ms(uint8_t btn);

/* 获取指定报告 ID 的原始数据（首字节为报告 ID），用于通过 USB 端点发送。
 * 返回报告长度，buf 为 NULL 或返回 0 表示报告 ID 无效。 */
uint16_t krpt_get_raw(uint8_t id, const uint8_t **buf);



#endif  /* KEYRPT_H */
