/**
 * @Author: wj
 * @Brief:  rgb .
 * @Date:   2024/07/02
 */
#include "rgb.h"
#include "db.h"

#define TAG "rgb: "

obr_t g_obr;



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
    return result;
}

