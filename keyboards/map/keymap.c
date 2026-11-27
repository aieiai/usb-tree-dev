#include "db.h"
#include "flash_layout.h"
#include "keymap.h"
#include "keycode.h"

#define TAG "keymap: "

obm_t g_obm;
keymap_t g_keymap = {0};

const uint8_t *keymap_conv_ki_list(void);
const uint8_t *keymap_conv_ks_list(void);

static void _keymap_load_defaults(void)
{
    obm_t *p_obm = &g_obm;

    memset(p_obm, 0, sizeof(*p_obm));
    p_obm->magic   = MAGIC_OBKM;
    p_obm->version = OBKM_VER;
    p_obm->sys_max = KBD_SYS_MAX;
    p_obm->map_max = KBD_MAP_MAX;
    p_obm->key_max = KBD_KEY_MAX;
    p_obm->sys_cur = 0;
    p_obm->map_cur = 0;

    for (uint32_t sys = 0U; sys < KBD_SYS_MAX; sys++) {
        for (uint32_t layer = 0U; layer < KBD_MAP_MAX; layer++) {
            const uint32_t *p_default = keymap_get_default((uint8_t)sys, (uint8_t)layer);
            if (p_default != NULL) {
                memcpy(p_obm->keymap[sys][layer], p_default, sizeof(p_obm->keymap[sys][layer]));
            }
        }
    }
}

static bool _keymap_codes_valid(const obm_t *p_obm)
{
    for (uint32_t sys = 0U; sys < KBD_SYS_MAX; sys++) {
        for (uint32_t layer = 0U; layer < KBD_MAP_MAX; layer++) {
            for (uint32_t key = 0U; key < KBD_KEY_MAX; key++) {
                rotkc_t *pkc = (rotkc_t *)&p_obm->keymap[sys][layer][key];
                if (!_KC_VALID(pkc)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool _keymap_valid(const obm_t *p_obm)
{
    bool layout_ok;

    if ((p_obm->magic != MAGIC_OBKM) || (p_obm->version != OBKM_VER) ||
        (p_obm->sys_max != KBD_SYS_MAX) || (p_obm->sys_cur >= KBD_SYS_MAX) || 
        (p_obm->map_max != KBD_MAP_MAX) || (p_obm->map_cur >= KBD_MAP_MAX) ||
        (p_obm->key_max != KBD_KEY_MAX)) {
        return false;
    }

    layout_ok = (p_obm->layout_max == 0U) ? (p_obm->layout_cur == 0U) : (p_obm->layout_cur < p_obm->layout_max);
    return layout_ok && _keymap_codes_valid(p_obm);
}


rotsta_t obm_db_register(void)
{
    rotsta_t result = rotsta_success;
    obm_t *p_obm = &g_obm;

    result = db_register(DB_ITEM_OBM, p_obm, (uint32_t)sizeof(g_obm));
    if (result != rotsta_success) {
        printf(TAG "obm register failed: 0x%08x\r\n", (unsigned)result);
        _keymap_load_defaults();
        return result;
    }

    if (!_keymap_valid(p_obm)) {
        printf(TAG "obm invalid (magic=0x%08x ver=%u), write defaults\r\n", (unsigned)p_obm->magic, (unsigned)p_obm->version);
        _keymap_load_defaults();
        result = db_update(DB_ITEM_OBM, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "obm default save failed: 0x%08x\r\n", (unsigned)result);
        }
    }

    return result;
}

rotsta_t keymap_reset(void)
{
    _keymap_load_defaults();
    return rotsta_success;
}


uint32_t keymap_get_key(uint8_t layer, uint8_t key)
{
    obm_t *p_obm = &g_obm;
    return p_obm->keymap[p_obm->sys_cur][layer][key];
}


rotsta_t keymap_set_key(uint8_t layer, uint8_t key, uint32_t code)
{
    obm_t *p_obm = &g_obm;
    rotkc_t *pkc = (rotkc_t *)&code;

#if 1
    switch (pkc->ty) {
        case KCT_KBD:
            printf(TAG "KBD - co: 0x%02x\r\n", (unsigned)pkc->co);
            break;
        case KCT_SYS:
            printf(TAG "SYS - co: 0x%02x\r\n", (unsigned)pkc->co);
            break;
        case KCT_CSM:
            printf(TAG "CSM - co: 0x%04x\r\n", (unsigned)pkc->co);
            break;
        case KCT_ADV:
            printf(TAG "ADV - sy: %d, co: 0x%02x\r\n", pkc->sy, (unsigned)pkc->co);
            break;
        case KCT_MS:
            printf(TAG "MS - sy: %d, co: 0x%04x\r\n", pkc->sy, (unsigned)pkc->co);
            break;
        case KCT_GP:
            printf(TAG "GP - sy: %d, co: 0x%04x\r\n", pkc->sy, (unsigned)pkc->co);
            break;
        case KCT_FN:
            printf(TAG "FN - sy: %d, fy: %d, co: 0x%04x\r\n", pkc->sy, pkc->fy, (unsigned)pkc->co);
            break;
        default:
            printf(TAG "kc Exception: kc:0x%08x\r\n", (unsigned)code);
            break;
    }
#endif

    if (!_KC_VALID(pkc)) {
        return rotsta_invalid_argument;
    }

    p_obm->keymap[p_obm->sys_cur][layer][key] = code;
    return rotsta_success;
}


uint8_t keymap_get_active_layer(void)
{
    return g_obm.map_cur;
}


rotsta_t keymap_set_active_layer(uint8_t layer)
{
    if (layer >= KBD_MAP_MAX) {
        return rotsta_invalid_argument;
    }
    g_obm.map_cur = layer;
    return rotsta_success;
}

uint8_t keymap_get_active_sys(void)
{
    return g_obm.sys_cur;
}


rotsta_t keymap_set_active_sys(uint8_t sys)
{
    if (sys >= KBD_SYS_MAX) {
        return rotsta_invalid_argument;
    }
    g_obm.sys_cur = sys;
    return rotsta_success;
}


rotsta_t keymap_init(void)
{
    keymap_t *p_map = &g_keymap;
    p_map->ki_list = keymap_conv_ki_list();
    p_map->ks_list = keymap_conv_ks_list();

    return rotsta_success;
}
