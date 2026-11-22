/**
 * @Author: wj
 * @Brief:  key advanced.
 * @Date:   2024/07/02
 */
#include <stddef.h>
#include <stdio.h>

#include "keyadv.h"
#include "db.h"

#define TAG "adv: "

adv_func_t adv_func[KADV_TY_MAX] = {NULL};
oba_t g_oba;

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
    return result;
}

