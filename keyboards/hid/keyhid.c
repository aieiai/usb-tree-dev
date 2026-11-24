#include "keyhid.h"
#include "keycode.h"
#include "rotutil.h"

static khid_rpt_nkro_t s_rpt_nkro = { .id = KHID_RPT_ID_NKRO };
static khid_rpt_csm_t  s_rpt_csm  = { .id = KHID_RPT_ID_CSM  };
static khid_rpt_sys_t  s_rpt_sys  = { .id = KHID_RPT_ID_SYS  };
static khid_rpt_6kro_t s_rpt_kb6  = { .id = KHID_RPT_ID_6KRO };
static khid_rpt_ms_t   s_rpt_ms   = { .id = KHID_RPT_ID_MS   };

static inline uint8_t khid_mod_bit(uint8_t kc)
{
    return (uint8_t)(0x01U << (kc - KC_LCTL));
}

/* System Control usage 0x81..0x83 映射到描述符 Logical 1..3，0 表示无动作 */
static inline uint8_t khid_sys_action(uint8_t sc)
{
    if ((sc >= SC_POWER_DOWN) && (sc <= SC_WAKE_UP)) {
        return (uint8_t)(sc - SC_POWER_DOWN + 1U);
    }
    return 0U;
}

/** ---------------- Report ID 4: 6KRO ---------------- */

void khid_rpt_add_6kro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_set(s_rpt_kb6.modify.dat, khid_mod_bit(kc));
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

void khid_rpt_del_6kro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_clr(s_rpt_kb6.modify.dat, khid_mod_bit(kc));
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

bool khid_rpt_get_6kro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        return rotutil_bit_get(s_rpt_kb6.modify.dat, khid_mod_bit(kc)) != 0U;
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

void khid_rpt_add_nkro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_set(s_rpt_nkro.modify.dat, khid_mod_bit(kc));
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(s_rpt_nkro.keys) * 8U))) {
        /* 位图 bit0 对应 usage 0；usage >= 128 不在描述符范围内 */
        rotutil_bit_set(s_rpt_nkro.keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U)));
    }
}

void khid_rpt_del_nkro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_clr(s_rpt_nkro.modify.dat, khid_mod_bit(kc));
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(s_rpt_nkro.keys) * 8U))) {
        rotutil_bit_clr(s_rpt_nkro.keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U)));
    }
}

bool khid_rpt_get_nkro(uint8_t kc)
{
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        return rotutil_bit_get(s_rpt_nkro.modify.dat, khid_mod_bit(kc)) != 0U;
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(s_rpt_nkro.keys) * 8U))) {
        return rotutil_bit_get(s_rpt_nkro.keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U))) != 0U;
    }
    return false;
}

/** ---------------- Report ID 2: Consumer Control ---------------- */

void khid_rpt_add_csm(uint16_t cc)
{
    if (cc != 0U) {
        s_rpt_csm.usage = cc;
    }
}

void khid_rpt_del_csm(uint16_t cc)
{
    if (s_rpt_csm.usage == cc) {
        s_rpt_csm.usage = 0U;
    }
}

bool khid_rpt_get_csm(uint16_t cc)
{
    return (cc != 0U) && (s_rpt_csm.usage == cc);
}

/** ---------------- Report ID 3: System Control ---------------- */

void khid_rpt_add_sys(uint8_t sc)
{
    uint8_t action = khid_sys_action(sc);
    if (action != 0U) {
        s_rpt_sys.action = action;
    }
}

void khid_rpt_del_sys(uint8_t sc)
{
    if ((khid_sys_action(sc) != 0U) && (s_rpt_sys.action == khid_sys_action(sc))) {
        s_rpt_sys.action = 0U;
    }
}

bool khid_rpt_get_sys(uint8_t sc)
{
    uint8_t action = khid_sys_action(sc);
    return (action != 0U) && (s_rpt_sys.action == action);
}

/** ---------------- Report ID 5: Mouse ---------------- */

void khid_rpt_add_ms(uint8_t btn)
{
    s_rpt_ms.buttons |= btn;
}

void khid_rpt_del_ms(uint8_t btn)
{
    s_rpt_ms.buttons &= (uint8_t)~btn;
}

bool khid_rpt_get_ms(uint8_t btn)
{
    return (btn != 0U) && ((s_rpt_ms.buttons & btn) != 0U);
}
