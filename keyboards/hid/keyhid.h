#ifndef KEYHID_H
#define KEYHID_H

#include <stdint.h>
#include <stdbool.h>

enum {
    KHID_RPT_ID_NKRO = 0x01,
    KHID_RPT_ID_CSM  = 0x02,
    KHID_RPT_ID_SYS  = 0x03,
    KHID_RPT_ID_6KRO = 0x04,
    KHID_RPT_ID_MS   = 0x05,
    KHID_RPT_ID_MAX
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
} __attribute__((packed)) khid_mod_t;

typedef struct {
    uint8_t    id;
    khid_mod_t modify;
    uint8_t    rsv;
    uint8_t    keys[6];
} __attribute__((packed)) khid_rpt_6kro_t;

typedef struct {
    uint8_t    id;
    khid_mod_t modify;
    uint8_t    rsv;
    uint8_t    keys[16];
} __attribute__((packed)) khid_rpt_nkro_t;

typedef struct {
    uint8_t  id;
    uint16_t usage;
} __attribute__((packed)) khid_rpt_csm_t;

typedef struct {
    uint8_t id;
    uint8_t action;
} __attribute__((packed)) khid_rpt_sys_t;

typedef struct {
    uint8_t id;
    uint8_t buttons;
    int16_t x;
    int16_t y;
    int16_t wheel;
    int16_t pan;
} __attribute__((packed)) khid_rpt_ms_t;

void khid_rpt_add_6kro(uint8_t kc);
void khid_rpt_del_6kro(uint8_t kc);
bool khid_rpt_get_6kro(uint8_t kc);

void khid_rpt_add_nkro(uint8_t kc);
void khid_rpt_del_nkro(uint8_t kc);
bool khid_rpt_get_nkro(uint8_t kc);

void khid_rpt_add_csm(uint16_t cc);
void khid_rpt_del_csm(uint16_t cc);
bool khid_rpt_get_csm(uint16_t cc);

void khid_rpt_add_sys(uint8_t sc);
void khid_rpt_del_sys(uint8_t sc);
bool khid_rpt_get_sys(uint8_t sc);

void khid_rpt_add_ms(uint8_t btn);
void khid_rpt_del_ms(uint8_t btn);
bool khid_rpt_get_ms(uint8_t btn);



#endif  /* KEYHID_H */
