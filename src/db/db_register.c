#include "db.h"
#include "rotsta.h"

#include "keyprop.h"
#include "keymap.h"
#include "keyadv.h"
#include "keymagnet.h"
#include "rgb.h"

#define TAG "db_reg: "

rotsta_t db_register_kbd(void)
{
    rotsta_t result = rotsta_success;

    result = obi_db_register();
    if (result != rotsta_success) {
        printf(TAG "obi_db_register failed: 0x%08x\r\n", (unsigned)result);
    }

    result = obm_db_register();
    if (result != rotsta_success) {
        printf(TAG "obm_db_register failed: 0x%08x\r\n", (unsigned)result);
    }

    result = obkp_db_register();
    if (result != rotsta_success) {
        printf(TAG "obkp_db_register failed: 0x%08x\r\n", (unsigned)result);
    }

    result = obmag_db_register();
    if (result != rotsta_success) {
        printf(TAG "obmag_db_register failed: 0x%08x\r\n", (unsigned)result);
    }

    result = oba_db_register();
    if (result != rotsta_success) {
        printf(TAG "oba_db_register failed: 0x%08x\r\n", (unsigned)result);
    }

    result = obr_db_register();
    if (result != rotsta_success) {
        printf(TAG "obr_db_register failed: 0x%08x\r\n", (unsigned)result);
    }

    return result;
}
