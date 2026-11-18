/**
 * @Author: wj
 * @Brief:  key report processing.
 * @Date:   2024/07/02
 */
#include "key_report.h"
#include "key_cfg.h"
#include "hal.h"


krpt_t krpt[RPT_MAX];

//static krpt_ms_t *rpt_ms   = (krpt_ms_t *)phy_spi.tx_buf;
static krpt_kb6_t *rpt_kb6 = (krpt_kb6_t *)phy_spi.tx_buf;
static krpt_kbn_t *rpt_kbn = (krpt_kbn_t *)phy_spi.tx_buf;
//static krpt_sys_t *rpt_sys = (krpt_sys_t *)phy_spi.tx_buf;

/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline bool krpt_kb_6kro_add(uint8_t kc)
{
    if (kc >= 0xE0 && kc <= 0xE7) // modify
    {
        BIT_SET(rpt_kb6->modify.dat, (0x01 << (kc - 0xE0)));
    }
    else
    {
        uint8_t size = sizeof(rpt_kb6->keys);
        for (uint8_t i = 0; i < size; i++)
        {
            if (rpt_kb6->keys[i] == 0)
            {
                BIT_WRT(rpt_kb6->keys[i], kc);
                break;
            }
        }
    }
    return 0;
}
static __inline bool krpt_kb_6kro_del(uint8_t kc)
{
    if (kc >= 0xE0 && kc <= 0xE7) // modify
    {
        BIT_CLR(rpt_kb6->modify.dat, (0x01 << (kc - 0xE0)));
    }
    else
    {
        uint8_t size = sizeof(rpt_kb6->keys);
        for (uint8_t i = 0; i < size; i++)
        {
            if (rpt_kb6->keys[i] == kc)
            {
                BIT_WRT(rpt_kb6->keys[i], 0);
                break;
            }
        }
    }
    return 0;
}
static __inline bool krpt_kb_6kro_get(uint8_t kc)
{
    bool find = false;
    if (kc >= 0xE0 && kc <= 0xE7) // modify
    {
        find = BIT_GET(rpt_kb6->modify.dat, (0x01 << (kc - 0xE0))) ? true : false;
    }
    else if (kc <= 0xDD)
    {
        uint8_t size = sizeof(rpt_kb6->keys);
        for (uint8_t i = 0; i < size; i++)
        {
            if (rpt_kb6->keys[i] == kc)
            {
                find = true;
                break;
            }
        }
    }
    return find;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline bool krpt_kb_nkro_add(uint8_t kc)
{
    if (kc < 0xA8)
    {
        BIT_SET(rpt_kbn->bit.k1[kc / 8], (0x01 << (kc % 8)));
    }
    else if (kc < 0xE8)
    {
        kc -= 0xB0;
        BIT_SET(rpt_kbn->bit.k2[kc / 8], (0x01 << (kc % 8)));
    }
    return 0;
}
static __inline bool krpt_kb_nkro_del(uint8_t kc)
{
    if (kc < 0xA8)
    {
        BIT_CLR(rpt_kbn->bit.k1[kc / 8], (0x01 << (kc % 8)));
    }
    else if (kc < 0xE8)
    {
        kc -= 0xB0;
        BIT_CLR(rpt_kbn->bit.k2[kc / 8], (0x01 << (kc % 8)));
    }
    return 0;
}
static __inline bool krpt_kb_nkro_get(uint8_t kc)
{
    bool find = false;
    if (kc < 0xA8)
    {
        find = BIT_GET(rpt_kbn->bit.k1[kc / 8], (0x01 << (kc % 8))) ? true : false;
    }
    else if (kc < 0xE8)
    {
        kc  -= 0xB0;
        find = BIT_GET(rpt_kbn->bit.k2[kc / 8], (0x01 << (kc % 8))) ? true : false;
    }
    return find;
}



/**
 * @brief:
 * @param:  None
 * @return: None
 */
void krpt_set_kb_rpt_mode(bool is_nkro)
{
    krpt[RPT_KB].len    = (is_nkro) ? sizeof(krpt_kbn_t) : sizeof(krpt_kb6_t);
    krpt[RPT_KB].fc_add = (is_nkro) ? krpt_kb_nkro_add : krpt_kb_6kro_add;
    krpt[RPT_KB].fc_del = (is_nkro) ? krpt_kb_nkro_del : krpt_kb_6kro_del;
    krpt[RPT_KB].fc_get = (is_nkro) ? krpt_kb_nkro_get : krpt_kb_6kro_get;
}
void krpt_set_ms_rpt_mode(bool is_open)
{
    krpt[RPT_MS].len    = sizeof(krpt_ms_t);
    krpt[RPT_MS].fc_add = NULL;
    krpt[RPT_MS].fc_del = NULL;
    krpt[RPT_MS].fc_get = NULL;
}
void krpt_set_sys_rpt_mode(bool is_open)
{
    krpt[RPT_KB].len    = sizeof(krpt_sys_t);
    krpt[RPT_KB].fc_add = NULL;
    krpt[RPT_KB].fc_del = NULL;
    krpt[RPT_KB].fc_get = NULL;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void krpt_handle(void)
{
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
krpt_t *krpt_init(void)
{
    bool nkro = (g_kcfg.info.nkro) ? true : false;
    krpt[RPT_KB].len    = (nkro) ? sizeof(krpt_kbn_t) : sizeof(krpt_kb6_t);
    krpt[RPT_KB].fc_add = (nkro) ? krpt_kb_nkro_add : krpt_kb_6kro_add;
    krpt[RPT_KB].fc_del = (nkro) ? krpt_kb_nkro_del : krpt_kb_6kro_del;
    krpt[RPT_KB].fc_get = (nkro) ? krpt_kb_nkro_get : krpt_kb_6kro_get;
    
    krpt[RPT_MS].len    = sizeof(krpt_ms_t);
    krpt[RPT_MS].fc_add = NULL;
    krpt[RPT_MS].fc_del = NULL;
    krpt[RPT_MS].fc_get = NULL;
    
    krpt[RPT_SYS].len    = sizeof(krpt_sys_t);
    krpt[RPT_SYS].fc_add = NULL;
    krpt[RPT_SYS].fc_del = NULL;
    krpt[RPT_SYS].fc_get = NULL;
    
    krpt[RPT_USER].len    = sizeof(krpt_user_t);
    krpt[RPT_USER].fc_add = NULL;
    krpt[RPT_USER].fc_del = NULL;
    krpt[RPT_USER].fc_get = NULL;
    
    return krpt;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void krpt_deinit(void)
{
}
