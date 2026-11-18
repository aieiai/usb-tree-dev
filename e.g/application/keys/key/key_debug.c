/**
 * @Author: wj
 * @Brief:  key debug.
 * @Date:   2024/07/02
 */
#include "key_debug.h"
#include "key_cfg.h"
#include "key_sta.h"
#include "hal.h"

kdbg_t kdbg[KEY_NUM];

extern ksta_t ksta[KEY_NUM];

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void kdbg_handle(void)
{
    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        bool update    = false;
        kdbg[ki].val   = ksta[ki].valn;
        kdbg[ki].mvolt = HAL_ADC_TO_MVOLT(kdbg[ki].val);
        if (kdbg[ki].val < kdbg[ki].min)
        {
            update = true;
            kdbg[ki].min = kdbg[ki].val;
        }
        if (kdbg[ki].val > kdbg[ki].max)
        {
            update = true;
            kdbg[ki].max = kdbg[ki].val;
        }
        if (update)
        {
            kdbg[ki].dif = kdbg[ki].max - kdbg[ki].min;
            kdbg[ki].mvolt_dif = HAL_ADC_TO_MVOLT(kdbg[ki].dif);
        }
        // kdbg[ki].min   = kdbg[ki].val < kdbg[ki].min ? kdbg[ki].val : kdbg[ki].min;
        // kdbg[ki].max   = kdbg[ki].val > kdbg[ki].max ? kdbg[ki].val : kdbg[ki].max;
        // kdbg[ki].dif   = kdbg[ki].max - kdbg[ki].min;
        // kdbg[ki].mvolt_dif = HAL_ADC_TO_MVOLT(kdbg[ki].dif);
    }
}
/**
 * @brief:
 * @param:  None
 * @return: None
 */
void kdbg_clear(uint8_t ki)
{
    kdbg[ki].clr = false;
    kdbg[ki].min = 0xffff;
    kdbg[ki].max = 0;
    kdbg[ki].val = 0;
    kdbg[ki].dif = 0;
    kdbg[ki].dst = 0;
    kdbg[ki].mvolt = 0;
    kdbg[ki].mvolt_dif = 0;
}
/**
 * @brief:
 * @param:  None
 * @return: None
 */
void kdbg_clears(void)
{
    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        kdbg_clear(ki);
    }
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void kdbg_clear_monitor(void)
{
    for (uint8_t ki = 0; ki < KEY_NUM; ki++)
    {
        if (kdbg[ki].clr)
        {
            kdbg_clear(ki);
        }
    }
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
kdbg_t *kdbg_init(void)
{
    kdbg_clears();
    return kdbg;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void kdbg_deinit(void)
{
}
