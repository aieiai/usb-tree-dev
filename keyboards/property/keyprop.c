#include "keyprop.h"
#include <stdio.h>

#include "db.h"

#include "rotver.h"

#define TAG "keyprop: "



obi_t g_obi;
obkp_t g_obkp[KBD_KEY_MAX];




static void _obi_set_str(uint8_t *dst, uint32_t size, const char *src)
{
    uint32_t i = 0U;

    while (((i + 1U) < size) && (src[i] != '\0')) {
        dst[i] = (uint8_t)src[i];
        i++;
    }
    dst[i] = 0U;
}

static void _obi_load_defaults(void)
{
    memset(&g_obi, 0, sizeof(g_obi));

    g_obi.magic   = MAGIC_OBI;
    g_obi.version = OBI_VER;
    _obi_set_str(g_obi.name, sizeof(g_obi.name), "Trace40");

    g_obi.fs.gp_mode = 0U;
    g_obi.fs.cc_popup = 0U;

    g_obi.pi.vid = 0x7989U;
    g_obi.pi.pid = 0x0001U;
    g_obi.pi.fv  = (uint64_t)FW_VER;
    g_obi.pi.hv  = ROTVER_MAKE(0U, 1U, 0U, ROTVER_STAGE_RELEASE, 0U);
    _obi_set_str(g_obi.pi.cd, sizeof(g_obi.pi.cd), rotver_get_cd());
    _obi_set_str(g_obi.pi.mn, sizeof(g_obi.pi.mn), "Ray");
    _obi_set_str(g_obi.pi.pn, sizeof(g_obi.pi.pn), "Trace40");
    _obi_set_str(g_obi.pi.sn, sizeof(g_obi.pi.sn), "SN000000000001");
    g_obi.pi.test_cnt = 0U;
}
static rotsta_t obi_init(void)
{
    rotsta_t result = db_register(DB_ITEM_TY_OBI, &g_obi, (uint32_t)sizeof(g_obi));
    if (result != rotsta_success) {
        printf(TAG "obi register failed: 0x%08x\r\n", (unsigned)result);
        return result;
    }

    if ((g_obi.magic != MAGIC_OBI) || (g_obi.version != OBI_VER)) {
        printf(TAG "kattr blank (magic=0x%08x ver=%u), write defaults\r\n", (unsigned)g_obi.magic, (unsigned)g_obi.version);
        _obi_load_defaults();
        result = db_update(DB_ITEM_TY_OBI, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "kattr default save failed: 0x%08x\r\n", (unsigned)result);
        }
    }
    else if (g_obi.pi.fv != (uint64_t)FW_VER) {
        uint64_t prev_fv = g_obi.pi.fv;

        /*
         * Firmware changed: migrate persistent data from prev_fv here before
         * the stored version is advanced. Keep the checks ordered by version,
         * e.g. if (prev_fv < ROTVER_MAKE(1, 5, 0, ROTVER_STAGE_RELEASE, 0)).
         */
        g_obi.pi.fv = (uint64_t)FW_VER;
        _obi_set_str(g_obi.pi.cd, sizeof(g_obi.pi.cd), rotver_get_cd());
        printf(TAG "kattr fw version 0x%08x%08x -> 0x%08x%08x\r\n",
               (unsigned)(prev_fv >> 32), (unsigned)prev_fv,
               (unsigned)(g_obi.pi.fv >> 32), (unsigned)g_obi.pi.fv);
        result = db_update(DB_ITEM_TY_OBI, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "kattr fw version save failed: 0x%08x\r\n", (unsigned)result);
        }
    }
    else if ((g_obi.pi.fv == (uint64_t)FW_VER) && strcmp((const char *)g_obi.pi.cd, rotver_get_cd()) != 0) {
        printf(TAG "kattr compile date %s != %s, update\r\n", (const char *)g_obi.pi.cd, rotver_get_cd());
        _obi_set_str(g_obi.pi.cd, sizeof(g_obi.pi.cd), rotver_get_cd());
        result = db_update(DB_ITEM_TY_OBI, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "kattr compile date save failed: 0x%08x\r\n", (unsigned)result);
        }
    }
    return result;
}


rotsta_t keyprop_init(void)
{
    return obi_init();
}