#include "keymagnet.h"
#include "keyaxis.h"
#include "keyprop.h"
#include "db.h"

#define TAG "keymagnet: "
#define KEYMAG_BOT_DFT  (0)
#define KEYMAG_TOP_DFT  ((0x01 << KBD_ADC_BIT) - 1)
#define KEYMAG_DIF_DFT  ((KBD_ADC_VEF / (float)((0x01 << KBD_ADC_BIT) - 1.0f) * 500)) /* 500mV */

obmag_t g_obmag;

typedef struct {
    float k;
    float b;
} factor_t;

static factor_t factor[KBD_KEY_MAX];

/**
 * @brief:  Convert the point location to the distance from the top of the key to the bottom.
 * @param:  The index of the key.
 * @param:  The point location to convert.
 * @return: The distance from the top of the key to the bottom in um.
 *
 *          The distance is calculated by the following formula.
 *          distance = (point - b) / k
 *          where b is the y-intercept of the line and k is the slope of the line.
 *          The line is defined by the key's attribute, such as the top and bottom values.
 */
float keymag_pt2du(uint8_t ki, uint16_t point)
{
    factor_t *p_factor = &factor[ki];
    keyattr_t *p_attr = &g_obkp.attr[ki];
    const keyaxis_para_t *p_axis = &g_keyaxis_para[p_attr->axis.ty];
    return (p_axis->t2h_dst - (p_factor->k / ((float)point - p_factor->b)));
}

/**
 * @brief:  Convert the distance from the top of the key to the bottom to the point location.
 * @param:  The index of the key.
 * @param:  The distance from the top of the key to the bottom in um.
 * @return: The point location.
 *
 *          The point location is calculated by the following formula.
 *          point = (distance * k) + b
 *          where b is the y-intercept of the line and k is the slope of the line.
 *          The line is defined by the key's attribute, such as the top and bottom values.
 */
uint16_t keymag_du2pt(uint8_t ki, float dist_um)
{
    factor_t *p_factor = &factor[ki];
    keyattr_t *p_attr = &g_obkp.attr[ki];
    const keyaxis_para_t *p_axis = &g_keyaxis_para[p_attr->axis.ty];
    return (uint16_t)((p_factor->k / (p_axis->t2h_dst - dist_um)) + p_factor->b);
}

/**
 * @brief:  Update the coefficient that calculates the distance between key.
 * @param:  The index of the key.
 * @return: None
 */
void keymag_update_factor(uint8_t ki)
{
    factor_t *p_factor = &factor[ki];
    keyattr_t *p_attr = &g_obkp.attr[ki];
    keymag_lmt_t *p_lmt = &g_obmag.cal.lmt[ki];
    const keyaxis_para_t *p_axis = &g_keyaxis_para[p_attr->axis.ty];

    float topv  = p_lmt->top;
    float botv  = p_lmt->bot;
    float t2hdm = p_axis->t2h_dst / 1000.0f;
    float b2hdm = p_axis->b2h_dst / 1000.0f;
    float t2bv  = topv - botv;
    float k     = (KBD_ADC_VEF / (float)((0x01 << KBD_ADC_BIT) - 1.0f) * t2bv) / ((1.0f / t2hdm) - (1.0f / b2hdm));
    float b     = (KBD_ADC_VEF / (float)((0x01 << KBD_ADC_BIT) - 1.0f) * topv) - (k / t2hdm);
    p_factor->k = (1000.0f * 1000.0f / (KBD_ADC_VEF * 1000.0f) * (float)((0x01 << KBD_ADC_BIT) - 1.0f)) * k;
    p_factor->b = (1000.0f * (float)((0x01 << KBD_ADC_BIT) - 1.0f) / (KBD_ADC_VEF * 1000.0f)) * b;
}

static bool _keymag_cal_valid(const keymag_cal_t *p_cal)
{
    if ((p_cal->magic != MAGIC_OBMAG) || (p_cal->version != OBMAG_VER)) {
        return false;
    }
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        if ((p_cal->lmt[ki].max <= p_cal->lmt[ki].min) || 
            (p_cal->lmt[ki].max - p_cal->lmt[ki].min) < KEYMAG_DIF_DFT) {
            return false;
        }
    }
    return true;
}

static void _keymag_cal_defaults(keymag_cal_t *p_cal, const char *name)
{
    memset(p_cal, 0, sizeof(*p_cal));
    p_cal->magic = MAGIC_OBMAG;
    p_cal->version = OBMAG_VER;
    (void)strncpy((char *)p_cal->name, name, sizeof(p_cal->name) - 1U);

    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        p_cal->lmt[ki].top = KEYMAG_TOP_DFT;
        p_cal->lmt[ki].bot = KEYMAG_BOT_DFT;
    }
}

static void _keymag_load_defaults(void)
{
    _keymag_cal_defaults(&g_obmag.cal, "Magnet");
    _keymag_cal_defaults(&g_obmag.cal_bak, "Magnet");
}

rotsta_t keymag_init(void)
{
    rotsta_t result = rotsta_success;

    result = db_register(DB_ITEM_OBMAG, &g_obmag, (uint32_t)sizeof(g_obmag));
    if (result != rotsta_success) {
        printf(TAG "obmag register failed: 0x%08x\r\n", (unsigned)result);
        return result;
    }

    if (!_keymag_cal_valid(&g_obmag.cal)) {
        printf(TAG "cal invalid (magic=0x%08x ver=%u), write defaults\r\n", (unsigned)g_obmag.cal.magic, (unsigned)g_obmag.cal.version);
        _keymag_load_defaults();
        result = db_update(DB_ITEM_OBMAG, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "cal default save failed: 0x%08x\r\n", (unsigned)result);
        }
    }
    else if (!_keymag_cal_valid(&g_obmag.cal_bak)) {
        printf(TAG "backup cal invalid, restore from cal\r\n");
        g_obmag.cal_bak = g_obmag.cal;
        result = db_update(DB_ITEM_OBMAG, DB_SAVE_TY_NOW);
        if (result != rotsta_success) {
            printf(TAG "backup cal save failed: 0x%08x\r\n", (unsigned)result);
        }
    }

    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        keymag_update_factor(ki);
    }
    return result;
}
