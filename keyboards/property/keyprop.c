#include "keyprop.h"

#include "db.h"

#include "rotver.h"

#define TAG "keyprop: "



obi_t g_obi;
obkp_t g_obkp;




static void _obi_set_str(uint8_t *dst, uint32_t size, const char *src)
{
    uint32_t i = 0U;

    while (((i + 1U) < size) && (src[i] != '\0')) {
        dst[i] = (uint8_t)src[i];
        i++;
    }
    dst[i] = 0U;
}

static bool _str_terminated(const uint8_t *str, uint32_t size)
{
    return (str != NULL) && (size != 0U) && (memchr(str, 0, size) != NULL);
}

static bool _obi_valid(void)
{
    if ((g_obi.pi.vid == 0U) || (g_obi.pi.pid == 0U) || 
        (g_obi.fs.gp_mode >= KPROP_GP_MAX) || 
        (g_obi.fs.link_mode >= KPROP_LINK_MAX) ||
        (g_obi.pm.en > 1U)) {
        return false;
    }
    return _str_terminated(g_obi.name, sizeof(g_obi.name)) &&
           _str_terminated(g_obi.pi.cd, sizeof(g_obi.pi.cd)) &&
           _str_terminated(g_obi.pi.mn, sizeof(g_obi.pi.mn)) &&
           _str_terminated(g_obi.pi.pn, sizeof(g_obi.pi.pn)) &&
           _str_terminated(g_obi.pi.sn, sizeof(g_obi.pi.sn));
}

static bool _obkp_valid(void)
{
    if ((g_obkp.magic != MAGIC_OBKP) || (g_obkp.version != OBKP_VER) ||
        (!_str_terminated(g_obkp.name, sizeof(g_obkp.name)))) {
        return false;
    }

    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        const keyattr_t *p_attr = &g_obkp.attr[ki];
        int trg = (int)p_attr->trg_ty;

        if ((trg < (int)KTRG_TY_NOM) || (trg >= (int)KTRG_TY_MAX)) {
            return false;
        }
        if (((uint32_t)p_attr->axis.ty >= (uint32_t)KAXIS_MAX) ||
            ((uint32_t)p_attr->axis.udef_ty >= (uint32_t)KAXIS_UDEF_MAX)) {
            return false;
        }
    }
    return true;
}

static void _obkp_load_single_defaults(uint8_t ki)
{
    keyattr_t *p_attr = &g_obkp.attr[ki];

    p_attr->trg_ty = KTRG_TY_NOM;
    p_attr->axis.ty = KAXIS_BB;
    p_attr->axis.udef_ty = KAXIS_UDEF0;
    
    p_attr->act.sclt  = OBKP_DFT_ACT_SCLT;
    p_attr->act.sclto = OBKP_DFT_ACT_SCLTO;
    p_attr->act.dclt  = OBKP_DFT_ACT_DCLT;
    p_attr->act.dclto = OBKP_DFT_ACT_DCLTO;
    
    p_attr->nom.dst_p = OBKP_DFT_NOM_DST_P;
    p_attr->nom.dst_r = OBKP_DFT_NOM_DST_R;
    
    p_attr->dz.dst_t = OBKP_DFT_DZ_DST_T;
    p_attr->dz.dst_b = OBKP_DFT_DZ_DST_B;
    
    p_attr->rt.dst_p = OBKP_DFT_RT_DST_P;
    p_attr->rt.dst_r = OBKP_DFT_RT_DST_R;
}
static void _obkp_load_defaults(void)
{
    memset(&g_obkp, 0, sizeof(g_obkp));
    g_obkp.magic = MAGIC_OBKP;
    g_obkp.version = OBKP_VER;
    _obi_set_str(g_obkp.name, sizeof(g_obkp.name), "Trace40");

    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        _obkp_load_single_defaults(ki);
    }
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
rotsta_t obi_db_register(void)
{
    rotsta_t result = db_register(DB_ITEM_OBI, &g_obi, (uint32_t)sizeof(g_obi));
    if (result != rotsta_success) {
        printf(TAG "obi register failed: 0x%08x\r\n", (unsigned)result);
        return result;
    }

    if ((g_obi.magic != MAGIC_OBI) || (g_obi.version != OBI_VER) || !_obi_valid()) {
        printf(TAG "obi invalid (magic=0x%08x ver=%u), write defaults\r\n", (unsigned)g_obi.magic, (unsigned)g_obi.version);
        _obi_load_defaults();
        result = db_update(DB_ITEM_OBI, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "obi default save failed: 0x%08x\r\n", (unsigned)result);
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
        printf(TAG "key property fw version 0x%08x%08x -> 0x%08x%08x\r\n", (unsigned)(prev_fv >> 32), (unsigned)prev_fv, (unsigned)(g_obi.pi.fv >> 32), (unsigned)g_obi.pi.fv);
        result = db_update(DB_ITEM_OBI, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "key property fw version save failed: 0x%08x\r\n", (unsigned)result);
        }
    }
    else if ((g_obi.pi.fv == (uint64_t)FW_VER) && strcmp((const char *)g_obi.pi.cd, rotver_get_cd()) != 0) {
        printf(TAG "key property compile date %s != %s, update\r\n", (const char *)g_obi.pi.cd, rotver_get_cd());
        _obi_set_str(g_obi.pi.cd, sizeof(g_obi.pi.cd), rotver_get_cd());
        result = db_update(DB_ITEM_OBI, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "key property compile date save failed: 0x%08x\r\n", (unsigned)result);
        }
    }
    return result;
}

rotsta_t obkp_db_register(void)
{
    rotsta_t result = db_register(DB_ITEM_OBKP, &g_obkp, (uint32_t)sizeof(g_obkp));
    if (result != rotsta_success) {
        printf(TAG "obkp register failed: 0x%08x\r\n", (unsigned)result);
        return result;
    }

    if (!_obkp_valid()) {
        printf(TAG "obkp invalid (magic=0x%08x ver=%u), write defaults\r\n", (unsigned)g_obkp.magic, (unsigned)g_obkp.version);
        _obkp_load_defaults();
        result = db_update(DB_ITEM_OBKP, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "obkp default save failed: 0x%08x\r\n", (unsigned)result);
        }
    }

    return result;
}

bool keyprop_cc_popup_enabled(void)
{
    return (g_obi.magic == MAGIC_OBI) && (g_obi.version == OBI_VER) && _obi_valid() && (g_obi.fs.cc_popup != 0U);
}

rotsta_t keyprop_init(void)
{
    return rotsta_success;
}