#include "db.h"
#include "flash_layout.h"
#include "keymap.h"
#include "keycode.h"

#define TAG "keymap: "

obm_t g_obm;

static void _keymap_load_defaults(void)
{
    obm_t *p_obm = &g_obm;
    p_obm->magic   = MAGIC_OBKM;
    p_obm->sys_max = KBD_SYS_MAX;
    p_obm->map_max = KBD_MAP_MAX;
    p_obm->key_max = KBD_KEY_MAX;
    p_obm->sys_cur = 0;
    p_obm->map_cur = 0;
    p_obm->crc32   = 0;

    for (uint32_t sys = 0U; sys < KBD_SYS_MAX; sys++) {
        for (uint32_t layer = 0U; layer < KBD_MAP_MAX; layer++) {
            memcpy(p_obm->keymap[sys][layer], keymap_get_default(sys, layer), sizeof(g_obm.keymap[sys][layer]));
        }
    }
}


rotsta_t keymap_init(void)
{
    bool blank = true;
    rotsta_t result = rotsta_success;
    obm_t *p_obm = &g_obm;

    result = db_register(DB_ITEM_OBM, p_obm, (uint32_t)sizeof(g_obm));
    if (result != rotsta_success) {
        printf(TAG "obm register failed: 0x%08x\r\n", (unsigned)result);
        _keymap_load_defaults();
        return result;
    }
    if (MAGIC_OBKM != p_obm->magic) {
        _keymap_load_defaults();
        blank = false;
    }

    if (blank) {
        _keymap_load_defaults();
        (void)db_update(DB_ITEM_OBM, DB_SAVE_TY_NOW);
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
