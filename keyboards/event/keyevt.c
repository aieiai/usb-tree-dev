/**
 * @Author: wj
 * @Brief:  key real-time event.
 * @Date:   2024/07/02
 */
#include "keyevt.h"
#include "keyaxis.h"
#include "keyprop.h"
#include "keymagnet.h"

#include <string.h>

keyevt_t g_keyevt[KBD_KEY_MAX] = {0};


/**
 * @brief:
 * @param:  None
 * @return: None
 */
void keyevt_init(void)
{
    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        keyevt_t *p_evt = &g_keyevt[ki];
        keyattr_t *p_attr = &g_obkp.attr[ki];
        
        memset(p_evt, 0, sizeof(keyevt_t));
        p_evt->pv = keymag_du2pt(ki, p_attr->nom.dst_p);
        p_evt->rv = keymag_du2pt(ki, p_attr->nom.dst_r);
        p_evt->axis_para = &g_keyaxis_para[p_attr->axis.ty];
    }
}

void keyevt_reload(void)
{
    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        keyevt_t *p_evt = &g_keyevt[ki];
        keyattr_t *p_attr = &g_obkp.attr[ki];
        
        p_evt->pv = keymag_du2pt(ki, p_attr->nom.dst_p);
        p_evt->rv = keymag_du2pt(ki, p_attr->nom.dst_r);
        p_evt->axis_para = &g_keyaxis_para[p_attr->axis.ty];
    }
}
