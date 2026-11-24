#include "keyrpt.h"
#include "keycode.h"
#include "rotutil.h"

#include <stddef.h>

static krpt_nkro_t s_rpt_nkro = { .id = KRPT_ID_NKRO };
static krpt_csm_t  s_rpt_csm  = { .id = KRPT_ID_CSM  };
static krpt_sys_t  s_rpt_sys  = { .id = KRPT_ID_SYS  };
static krpt_6kro_t s_rpt_kb6  = { .id = KRPT_ID_6KRO };
static krpt_ms_t   s_rpt_ms   = { .id = KRPT_ID_MS   };

static inline uint8_t krpt_mod_bit(uint8_t kc)
{
    return (uint8_t)(0x01U << (kc - KC_LCTL));
}

/* System Control usage 0x81..0x83 映射到描述符 Logical 1..3，0 表示无动作 */
static inline uint8_t krpt_sys_action(uint8_t sc)
{
    if ((sc >= SC_POWER_DOWN) && (sc <= SC_WAKE_UP)) {
        return (uint8_t)(sc - SC_POWER_DOWN + 1U);
    }
    return 0U;
}

/** ---------------- Report ID 4: 6KRO ---------------- */

void krpt_add_6kro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_set(s_rpt_kb6.modify.dat, krpt_mod_bit(kc));
    }
    else if (kc != KC_NONE) {
        for (uint8_t i = 0U; i < (uint8_t)sizeof(s_rpt_kb6.keys); i++) {
            if (s_rpt_kb6.keys[i] == KC_NONE) {
                s_rpt_kb6.keys[i] = kc;
                break;
            }
        }
    }
}

void krpt_del_6kro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_clr(s_rpt_kb6.modify.dat, krpt_mod_bit(kc));
    }
    else if (kc != KC_NONE) {
        for (uint8_t i = 0U; i < (uint8_t)sizeof(s_rpt_kb6.keys); i++) {
            if (s_rpt_kb6.keys[i] == kc) {
                s_rpt_kb6.keys[i] = KC_NONE;
                break;
            }
        }
    }
}

bool krpt_get_6kro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        return rotutil_bit_get(s_rpt_kb6.modify.dat, krpt_mod_bit(kc)) != 0U;
    }
    else if (kc != KC_NONE) {
        for (uint8_t i = 0U; i < (uint8_t)sizeof(s_rpt_kb6.keys); i++) {
            if (s_rpt_kb6.keys[i] == kc) {
                return true;
            }
        }
    }
    return false;
}

/** ---------------- Report ID 1: NKRO ---------------- */

void krpt_add_nkro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_set(s_rpt_nkro.modify.dat, krpt_mod_bit(kc));
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(s_rpt_nkro.keys) * 8U))) {
        /* 位图 bit0 对应 usage 0；usage >= 128 不在描述符范围内 */
        rotutil_bit_set(s_rpt_nkro.keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U)));
    }
}

void krpt_del_nkro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_clr(s_rpt_nkro.modify.dat, krpt_mod_bit(kc));
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(s_rpt_nkro.keys) * 8U))) {
        rotutil_bit_clr(s_rpt_nkro.keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U)));
    }
}

bool krpt_get_nkro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        return rotutil_bit_get(s_rpt_nkro.modify.dat, krpt_mod_bit(kc)) != 0U;
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(s_rpt_nkro.keys) * 8U))) {
        return rotutil_bit_get(s_rpt_nkro.keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U))) != 0U;
    }
    return false;
}

/** ---------------- Report ID 2: Consumer Control ---------------- */

void krpt_add_csm(uint16_t cc)
{
    if (cc != 0U) {
        s_rpt_csm.usage = cc;
    }
}

void krpt_del_csm(uint16_t cc)
{
    if (s_rpt_csm.usage == cc) {
        s_rpt_csm.usage = 0U;
    }
}

bool krpt_get_csm(uint16_t cc)
{
    return (cc != 0U) && (s_rpt_csm.usage == cc);
}

/** ---------------- Report ID 3: System Control ---------------- */

void krpt_add_sys(uint8_t sc)
{
    uint8_t action = krpt_sys_action(sc);
    if (action != 0U) {
        s_rpt_sys.action = action;
    }
}

void krpt_del_sys(uint8_t sc)
{
    if ((krpt_sys_action(sc) != 0U) && (s_rpt_sys.action == krpt_sys_action(sc))) {
        s_rpt_sys.action = 0U;
    }
}

bool krpt_get_sys(uint8_t sc)
{
    uint8_t action = krpt_sys_action(sc);
    return (action != 0U) && (s_rpt_sys.action == action);
}

/** ---------------- Report ID 5: Mouse ---------------- */

void krpt_add_ms(uint8_t btn)
{
    s_rpt_ms.buttons |= btn;
}

void krpt_del_ms(uint8_t btn)
{
    s_rpt_ms.buttons &= (uint8_t)~btn;
}

bool krpt_get_ms(uint8_t btn)
{
    return (btn != 0U) && ((s_rpt_ms.buttons & btn) != 0U);
}

uint16_t krpt_get_raw(uint8_t id, const uint8_t **buf)
{
    if (buf == NULL) {
        return 0U;
    }

    switch (id) {
        case KRPT_ID_NKRO:
            *buf = (const uint8_t *)&s_rpt_nkro;
            return (uint16_t)sizeof(s_rpt_nkro);
        case KRPT_ID_CSM:
            *buf = (const uint8_t *)&s_rpt_csm;
            return (uint16_t)sizeof(s_rpt_csm);
        case KRPT_ID_SYS:
            *buf = (const uint8_t *)&s_rpt_sys;
            return (uint16_t)sizeof(s_rpt_sys);
        case KRPT_ID_6KRO:
            *buf = (const uint8_t *)&s_rpt_kb6;
            return (uint16_t)sizeof(s_rpt_kb6);
        case KRPT_ID_MS:
            *buf = (const uint8_t *)&s_rpt_ms;
            return (uint16_t)sizeof(s_rpt_ms);
        default:
            *buf = NULL;
            return 0U;
    }
}
