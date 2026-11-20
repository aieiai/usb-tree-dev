#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "db.h"
#include "flash_layout.h"
#include "keymap.h"
#include "keycode.h"


obmap_t g_obmap;

static void _keymap_load_defaults(void)
{
    for (uint32_t sys = 0U; sys < KBD_SYS_MAX; sys++) {
        for (uint32_t layer = 0U; layer < KBD_MAP_MAX; layer++) {
            memcpy(g_obmap.keymap[sys][layer], keymap_get_default(sys, layer), sizeof(g_obmap.keymap[sys][layer]));
        }
    }
}


rotsta_t keymap_init(void)
{
    bool blank = true;
    rotsta_t state;

    state = db_register(DB_ITEM_TY_MAP, &g_obmap, (uint32_t)sizeof(g_obmap));
    if (state != rotsta_success) {
        _keymap_load_defaults();
        return state;
    }

    for (uint32_t sys = 0U; sys < KBD_SYS_MAX; sys++) {
        for (uint32_t layer = 0U; layer < KBD_MAP_MAX; layer++) {
            for (uint32_t key = 0U; key < KBD_KEY_MAX; key++) {
                if (g_obmap.keymap[sys][layer][key] != FLASH_INVALID_DAT) {
                    blank = false;
                    break;
                }
            }
        }
    }
    if (blank) {
        _keymap_load_defaults();
        (void)db_update(DB_ITEM_TY_MAP, DB_SAVE_TY_D5S);
    }

    g_obmap.map_cur = 0U;
    return rotsta_success;
}


rotsta_t keymap_reset(void)
{
    _keymap_load_defaults();
    g_obmap.map_cur = 0U;
    return rotsta_success;
}


uint32_t keymap_get_key(uint8_t layer, uint8_t key)
{
    /* 协议当前只操作系统 0 的映射 */
    return g_obmap.keymap[0][layer][key];
}


rotsta_t keymap_set_key(uint8_t layer, uint8_t key, uint32_t code)
{
    /* 协议当前只操作系统 0 的映射 */
    g_obmap.keymap[0][layer][key] = code;
    return rotsta_success;
}


uint8_t keymap_get_active_layer(void)
{
    return g_obmap.map_cur;
}


rotsta_t keymap_set_active_layer(uint8_t layer)
{
    if (layer >= KBD_MAP_MAX) {
        return rotsta_invalid_argument;
    }
    g_obmap.map_cur = layer;
    return rotsta_success;
}
