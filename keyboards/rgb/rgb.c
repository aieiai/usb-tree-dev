/**
 * @Author: wj
 * @Brief:  rgb .
 * @Date:   2024/07/02
 */
#include "rgb.h"
#include "db.h"

#define TAG "rgb: "

obr_t g_obr;

static bool _obr_valid(void)
{
    return (g_obr.magic == MAGIC_OBR) &&
           (g_obr.version == OBR_VER) &&
           (memchr(g_obr.name, 0, sizeof(g_obr.name)) != NULL);
}

static void _obr_load_defaults(void)
{
    memset(&g_obr, 0, sizeof(g_obr));
    g_obr.magic = MAGIC_OBR;
    g_obr.version = OBR_VER;
    (void)strncpy((char *)g_obr.name, "RGB", sizeof(g_obr.name) - 1U);
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
rotsta_t rgb_init(void)
{
    rotsta_t result = rotsta_success;
    result = db_register(DB_ITEM_OBR, &g_obr, (uint32_t)sizeof(g_obr));
    if (result != rotsta_success) {
        printf(TAG "obr register failed: 0x%08x\r\n", (unsigned)result);
        return result;
    }

    if (!_obr_valid()) {
        printf(TAG "obr invalid (magic=0x%08x ver=%u), write defaults\r\n", (unsigned)g_obr.magic, (unsigned)g_obr.version);
        _obr_load_defaults();
        result = db_update(DB_ITEM_OBR, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "obr default save failed: 0x%08x\r\n", (unsigned)result);
        }
    }

    return result;
}

