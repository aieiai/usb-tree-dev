/**
 * @Author: wj
 * @Brief:  key application processing.
 * @Date:   2024/07/02
 */
#include "key.h"

#include "key_sta.h"
#include "key_rt.h"
#include "key_adv.h"
#include "key_cfg.h"
#include "key_calib.h"
#include "key_xfer.h"
#include "key_report.h"
#include "key_check.h"
#include "key_debug.h"

#include "hal.h"
#include "board.h"
#include "lpf.h"

tu_t tu;
key_t key;
kstep_t kstep = {
    .fst = false,
    .cmd = KSTEP_TYPE_IDLE,
    .now = KSTEP_TYPE_IDLE,
    .pre = KSTEP_TYPE_IDLE,
};

float trun[20] = {0};

extern kcal_t kcal;
extern kdbg_t kdbg[KEY_NUM];
extern ksta_t ksta[KEY_NUM];
extern krt_t krt[KEY_NUM];
extern krpt_t krpt[RPT_MAX];


/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline void ksp_begin(void *user)
{
//    phy_tmr_t *msg_tmr = (phy_tmr_t *)user;
    // KEY_SET_STEP(KSTEP_TYPE_SAMPLE_BEGIN);
//    TPTX_T();
//    TPRX_T();
}

extern uint16_t key_analog[64];
const uint8_t keym_idx[10] = {0x00, 0x01, 0x02, 0x03, 14, 28, 41, 55, 56, 57};

/**
 * @brief:  Process the key data.
 *
 *          This function will process the key data in the buffer, calculate the
 *          current value, update the key state, and detect the trigger event.
 *
 * @param:  The pointer to the buffer of the key data.
 * @param:  The length of the buffer.
 * @param:  The user data.
 * @return: None
 */
static __inline void ksp_end_process(void *user)
{
    phy_adc_t *msg_adc = (phy_adc_t *)user;
//    hal_dwt_start();
//    
    const uint8_t *p_key_idx = keym_idx;
    uint16_t *p_analog = key_analog;
    for (uint8_t i = 0; i < msg_adc->len; i++)
    {
        p_analog[p_key_idx[i]] = msg_adc->buf[i];
    }
    tu.sampleTime = hal_dwt_end_us();
//    trun[0] = hal_dwt_end_us();
//    for (uint8_t ki = 0; ki < msg_adc->len; ki++)
//    {
//        // msg_adc->buf_ft[ki] = lpf_update(buf[ki], msg_adc->buf_ft[ki]);
//        msg_adc->buf_ft[ki] = msg_adc->buf[ki];
//        ksta_updete(ki, msg_adc->buf_ft[ki]);
//        switch (g_kcfg.attr[ki].type_op)
//        {
//            case KOP_TYPE_NOM: 
//                break;
//            case KOP_TYPE_RT: 
//                if (g_kcfg.attr[ki].rt.en)
//                {
//                    krt_process(ki);
//                }
//                break;
//            case KOP_TYPE_ADV: 
//                switch (g_kcfg.attr[ki].adv.type)
//                {
//                    case KADV_TYPE_SHORTCUT:
//                        break;
//                    case KADV_TYPE_REPLACE:
//                        break;
//                    case KADV_TYPE_MACRO:
//                        break;
//                    case KADV_TYPE_MT:
//                        break;
//                    case KADV_TYPE_TGL:
//                        break;
//                    case KADV_TYPE_DKS:
//                        break;
//                    default:
//                        break;
//                }
//                break;
//        }
//        
//        if (phy_spi.tx_busy == false)
//        {
//            if (krt[ki].evt_trg == KRT_EVT_TRG_D2U)
//            {
//                    krpt[RPT_KB].fc_del(g_kcfg.info.map[1][ki]);
//            }
//            else if (krt[ki].evt_trg == KRT_EVT_TRG_U2D)
//            {
//                krpt[RPT_KB].fc_add(g_kcfg.info.map[1][ki]);
//            }
//        }
//    }

//    trun[0] = hal_dwt_end_us();


    /*
    logd("k:%s, %s, %s, %s, %04d \r\n",
            (ksta[0].evt_ent == KSTA_EVT_ENT_U2U) ? "E_U2U" : "E_D2D",
            (ksta[0].evt_trg == KSTA_EVT_TRG_U2U) ? "T_U2U"
          : (ksta[0].evt_trg == KSTA_EVT_TRG_U2D) ? "T_U2D"
          : (key.evt_trg == KSTA_EVT_TRG_D2D) ? "T_D2D" : "T_D2U",

            (krt[0].evt_ent == KRT_EVT_ENT_U2U) ? "RE_U2U" : "RE_D2D",
            (krt[0].evt_trg == KRT_EVT_TRG_U2U) ? "RT_U2U"
          : (krt[0].evt_trg == KRT_EVT_TRG_U2D) ? "RT_U2D"
          : (krt[0].evt_trg == KRT_EVT_TRG_D2D) ? "RT_D2D" : "RT_D2U",

            ksta[0].dst
        );
        */
    // memcpy(&phy_spi.tx_buf[1], msg_adc->buf, sizeof(msg_adc->buf));
}

static __inline void ksp_end_pwr_impur(void *user)
{
//    phy_adc_t *msg_adc = (phy_adc_t *)user;
    static uint32_t cnt = 0;
    cnt++;
    if (cnt++ < 16000)
    {
        return;
    }
    else if (cnt++ < 32000)
    {
        // for (uint8_t ki = 0; ki < len; ki++)
        // {
        //     ksta_updete(ki, buf[ki]);
        //     krt_sta_rst_force(ki);
        // }
    }
    else
    {
        cnt = 0;
        hal_register_adc_full_cb(ksp_end_process);
    }
}

static __inline void key_rpt_fix_process(void *user)
{
}

static __inline bool key_rpt_give_wait_process(void *user)
{
    return false;
}
static __inline void key_rpt_take_done_process(void *user)
{
}

// bool key_get_step_fst(void)
// {
//     bool is_fst = key.step_fst;
//     if (is_fst)
//     {
//         key.step_fst = false;
//     }
//     return is_fst;
// }

void key_init(void)
{
    ksta_init();
    krt_init();
    kcalib_init();
    krpt_init();
    kdbg_init();
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void key_deinit(void)
{
   // hal_register_tmr_over_cb(NULL);
    hal_register_adc_full_cb(NULL);
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void key_main(void)
{
  //  hal_register_tmr_over_cb(ksp_begin);
    hal_register_adc_full_cb(ksp_end_pwr_impur);
    kxfer_init();

    kcfg_init();
    key_init();

    while (0)
    {
        kstep.fst = (kstep.now != kstep.pre) ? true : false; // step change
        kstep.pre = kstep.now; // update step pre
        kstep.now = kstep.cmd; //update step
        switch (kstep.now)
        {
            case KSTEP_TYPE_IDLE:
                break;

            case KSTEP_TYPE_INIT:
                key_init();
                kstep.cmd = KSTEP_TYPE_IDLE;
                break;

            case KSTEP_TYPE_CHECK:
                break;

            case KSTEP_TYPE_SAMPLE_BEGIN:
                break;

            case KSTEP_TYPE_SAMPLING:
                break;

            case KSTEP_TYPE_FILTER:
                break;

            case KSTEP_TYPE_SAMPLE_END:
                kstep.cmd = KSTEP_TYPE_IDLE;
                break;

            case KSTEP_TYPE_RPT_FIX:
                key_rpt_fix_process(NULL);
                kstep.cmd = KSTEP_TYPE_RPT_GIVE_WAIT;
                break;

            case KSTEP_TYPE_RPT_GIVE_WAIT:
                if (key_rpt_give_wait_process(NULL))
                {
                    kstep.cmd = KSTEP_TYPE_RPT_TAKE_DONE;
                }
                break;

            case KSTEP_TYPE_RPT_TAKE_DONE:
                key_rpt_take_done_process(NULL);
                break;

            case KSTEP_TYPE_CALIB:
                kcalib_process();
                if (kcal.res->res != KCAL_RES_KEEP)
                {
                    kstep.cmd = (kcal.res->res == KCAL_RES_EXIT_RELOAD) ? KSTEP_TYPE_INIT : KSTEP_TYPE_IDLE;
                }
                break;

            case KSTEP_TYPE_CALIB_AUTO:
                break;

            case KSTEP_TYPE_LAMP_SET:
                break;

            case KSTEP_TYPE_LAMP_MODE_SWITCH:
                break;

            case KSTEP_TYPE_PARA_RELOAD:
                break;

            case KSTEP_TYPE_SAVE:
                break;

            case KSTEP_TYPE_SLEEP:
                break;

            case KSTEP_TYPE_WAKEUP:
                break;

            case KSTEP_TYPE_TESTING:
                kstep.cmd = KSTEP_TYPE_IDLE;
                break;

            default:
                break;
        }
#if 0
        kdbg_clear_monitor();
        kdbg_handle();
#endif
    }
}
