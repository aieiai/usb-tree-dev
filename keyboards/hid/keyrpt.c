#include "keyrpt.h"
#include "keycode.h"
#include "rotutil.h"

#include <stddef.h>
#include <string.h>

keyrpt_ctx_t g_keyrpt_ctx = {
    .kro.id  = KRPT_ID_6KRO,
    .nkro.id = KRPT_ID_NKRO,
    .csm.id  = KRPT_ID_CSM,
    .sys.id  = KRPT_ID_SYS,
    .ms.id   = KRPT_ID_MS,
    .dirty_mask = 0U,
};


/** ---------------- Report ID 4: 6KRO ---------------- */

void keyrpt_6kro_add(uint8_t kc)
{
    keyrpt_6kro_t *p_6kro = &g_keyrpt_ctx.kro;
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_set(p_6kro->mod.dat, _MOD_BIT(kc));
    }
    else {
        for (uint8_t i = 0U; i < 6; i++) {
            if (p_6kro->keys[i] == 0) {
                p_6kro->keys[i] = kc;
                break;
            }
        }
    }
}

void keyrpt_6kro_del(uint8_t kc)
{
    keyrpt_6kro_t *p_6kro = &g_keyrpt_ctx.kro;
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_clr(p_6kro->mod.dat, _MOD_BIT(kc));
    }
    else {
        for (uint8_t i = 0U; i < 6; i++) {
            if (p_6kro->keys[i] == kc) {
                p_6kro->keys[i] = 0;
                break;
            }
        }
    }
}

bool keyrpt_6kro_get(uint8_t kc)
{
    keyrpt_6kro_t *p_6kro = &g_keyrpt_ctx.kro;
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        return rotutil_bit_get(p_6kro->mod.dat, _MOD_BIT(kc)) != 0U;
    }
    else if (kc != KC_NONE) {
        for (uint8_t i = 0U; i < (uint8_t)sizeof(p_6kro->keys); i++) {
            if (p_6kro->keys[i] == kc) {
                return true;
            }
        }
    }
    return false;
}

/** ---------------- Report ID 1: NKRO ---------------- */

void keyrpt_nkro_add(uint8_t kc)
{
    keyrpt_nkro_t *p_nkro = &g_keyrpt_ctx.nkro;
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_set(p_nkro->mod.dat, _MOD_BIT(kc));
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(p_nkro->keys) * 8U))) {
        /* 位图 bit0 对应 usage 0；usage >= 128 不在描述符范围内 */
        rotutil_bit_set(p_nkro->keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U)));
    }
}

void keyrpt_nkro_del(uint8_t kc)
{
    keyrpt_nkro_t *p_nkro = &g_keyrpt_ctx.nkro;
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        rotutil_bit_clr(p_nkro->mod.dat, _MOD_BIT(kc));
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(p_nkro->keys) * 8U))) {
        rotutil_bit_clr(p_nkro->keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U)));
    }
}

bool keyrpt_nkro_get(uint8_t kc)
{
    keyrpt_nkro_t *p_nkro = &g_keyrpt_ctx.nkro;
    if (kc >= KC_LCTL && kc <= KC_RGUI) {
        return rotutil_bit_get(p_nkro->mod.dat, _MOD_BIT(kc)) != 0U;
    }
    else if ((kc != KC_NONE) && (kc < (uint8_t)(sizeof(p_nkro->keys) * 8U))) {
        return rotutil_bit_get(p_nkro->keys[kc >> 3], (uint8_t)(0x01U << (kc & 0x07U))) != 0U;
    }
    return false;
}

/** ---------------- Report ID 2: Consumer Control ---------------- */

void keyrpt_csm_add(uint16_t cc)
{
    if (cc != 0U) {
        g_keyrpt_ctx.csm.co = cc;
    }
}

void keyrpt_csm_del(uint16_t cc)
{
    keyrpt_csm_t *p_csm = &g_keyrpt_ctx.csm;
    if (p_csm->co == cc) {
        p_csm->co = 0U;
    }
}

bool keyrpt_csm_get(uint16_t cc)
{
    return (cc != 0U) && (g_keyrpt_ctx.csm.co == cc);
}

/** ---------------- Report ID 3: System Control ---------------- */

void keyrpt_sys_add(uint8_t sc)
{
    if (!_SYS_VALID(sc)) {
        return;
    }
    
    g_keyrpt_ctx.sys.co = sc;
}

void keyrpt_sys_del(uint8_t sc)
{
    keyrpt_sys_t *p_sys = &g_keyrpt_ctx.sys;

    if (p_sys->co == sc) {
        p_sys->co = 0U;
    }
}

bool keyrpt_sys_get(uint8_t sc)
{
    return g_keyrpt_ctx.sys.co == sc;
}

/** ---------------- Report ID 5: Mouse ---------------- */

void keyrpt_ms_add_btn(uint8_t btn)
{
    g_keyrpt_ctx.ms.btn |= btn;
}

void keyrpt_ms_del_btn(uint8_t btn)
{
    g_keyrpt_ctx.ms.btn &= (uint8_t)~btn;
}

bool keyrpt_ms_get_btn(uint8_t btn)
{
    return (btn != 0U) && ((g_keyrpt_ctx.ms.btn & btn) != 0U);
}

void keyrpt_ms_set_move(int16_t dx, int16_t dy)
{
    keyrpt_ms_t *p_ms= &g_keyrpt_ctx.ms;
    p_ms->x = dx;
    p_ms->y = dy;
}

void keyrpt_ms_set_wheel(int16_t dw)
{
    g_keyrpt_ctx.ms.w = dw;
}

void keyrpt_ms_clr(void)
{
    keyrpt_ms_t *p_ms= &g_keyrpt_ctx.ms;
    p_ms->btn = 0U;
    p_ms->x = 0;
    p_ms->y = 0;
    p_ms->w = 0;
    p_ms->pan = 0;
}

uint16_t keyrpt_get_raw(uint8_t id, const uint8_t **buf)
{
    if (buf == NULL) {
        return 0U;
    }

    switch (id) {
        case KRPT_ID_NKRO:
            *buf = (const uint8_t *)&g_keyrpt_ctx.nkro;
            return (uint16_t)sizeof(g_keyrpt_ctx.nkro);
        case KRPT_ID_CSM:
            *buf = (const uint8_t *)&g_keyrpt_ctx.csm;
            return (uint16_t)sizeof(g_keyrpt_ctx.csm);
        case KRPT_ID_SYS:
            *buf = (const uint8_t *)&g_keyrpt_ctx.sys;
            return (uint16_t)sizeof(g_keyrpt_ctx.sys);
        case KRPT_ID_6KRO:
            *buf = (const uint8_t *)&g_keyrpt_ctx.kro;
            return (uint16_t)sizeof(g_keyrpt_ctx.kro);
        case KRPT_ID_MS:
            *buf = (const uint8_t *)&g_keyrpt_ctx.ms;
            return (uint16_t)sizeof(g_keyrpt_ctx.ms);
        default:
            *buf = NULL;
            return 0U;
    }
}
