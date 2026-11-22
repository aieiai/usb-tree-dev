#include "keymagnet.h"
#include "keyaxis.h"
#include "keyprop.h"
#include "db.h"

#define TAG "keymagnet: "

obmag_t g_obmag = {
    .cal = { .lmt[0] = { .top = 46248, .bot = 9664 } },
};

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

rotsta_t keymag_init(void)
{
    rotsta_t result = rotsta_success;
    result = db_register(DB_ITEM_OBMAG, &g_obmag, (uint32_t)sizeof(g_obmag));
    if (result != rotsta_success) {
        printf(TAG "obmag register failed: 0x%08x\r\n", (unsigned)result);
        return result;
    }

    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        keymag_update_factor(ki);
    }
    return result;
}
