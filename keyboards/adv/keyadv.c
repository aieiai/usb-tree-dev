/**
 * @Author: wj
 * @Brief:  key advanced.
 * @Date:   2024/07/02
 */
#include "keyadv.h"
#include "db.h"

#define TAG "adv: "

adv_func_t adv_func[KADV_TY_MAX] = {NULL};
oba_t g_oba;

static bool _oba_valid(void)
{
    return (g_oba.magic == MAGIC_OBA) && (g_oba.version == OBA_VER) && (memchr(g_oba.name, 0, sizeof(g_oba.name)) != NULL);
}

static void _oba_load_defaults(void)
{
    memset(&g_oba, 0, sizeof(g_oba));
    g_oba.magic = MAGIC_OBA;
    g_oba.version = OBA_VER;
    (void)strncpy((char *)g_oba.name, "Advanced", sizeof(g_oba.name) - 1U);
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
rotsta_t keyadv_init(void)
{
    rotsta_t result = rotsta_success;
    result = db_register(DB_ITEM_OBA, &g_oba, (uint32_t)sizeof(g_oba));
    if (result != rotsta_success) {
        printf(TAG "oba register failed: 0x%08x\r\n", (unsigned)result);
        return result;
    }

    if (!_oba_valid()) {
        printf(TAG "oba invalid (magic=0x%08x ver=%u), write defaults\r\n", (unsigned)g_oba.magic, (unsigned)g_oba.version);
        _oba_load_defaults();
        result = db_update(DB_ITEM_OBA, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "oba default save failed: 0x%08x\r\n", (unsigned)result);
        }
    }

    return result;
}

