#include "cst.h"

#include "hal_usb.h"


#include "db.h"
#include "rotver.h"

#include "keyprop.h"

#include "tx_api.h"
#include "tx_config.h"

#include "board.h"
#include "hpm_ppor_drv.h"
#include "hpm_gpio_drv.h"


#define TAG "cst"

static TX_THREAD cst_thread;
static uint8_t cst_thread_stack[CST_THREAD_STACK] __attribute__((aligned(8U)));

static void cst_obi_log(const char *tag)
{
    uint64_t fv = g_obi.pi.fv;

    hal_dt_printf("%s: name=%s, vid=0x%04x, pid=0x%04x, cnt=%u, fw=v%u.%u.%u-%s+%08x\r\n",
                  tag, (const char *)g_obi.name, (unsigned)g_obi.pi.vid, (unsigned)g_obi.pi.pid,
                  (unsigned)g_obi.pi.test_cnt,
                  ROTVER_GET_MAJOR(fv), ROTVER_GET_MINOR(fv), ROTVER_GET_PATCH(fv),
                  rotver_get_stage_str(ROTVER_GET_STAGE(fv)), (unsigned)ROTVER_GET_HASH(fv));
}

static void cst_thread_entry(ULONG id)
{
    (void)id;

    if (keyprop_init() != rotsta_success) {
        hal_dt_printf("[obi] init failed\r\n");
    } else {
        cst_obi_log("[obi] load");
    }

    uint32_t cnt = 0;

    for (;;) {
        
        // if ( cnt >= 1000 )
        // {
        //     cnt = 0;
        //     /* DT OUT 端点：上位机下发的数据，这里回显一下，方便验证双向通道 */
        //     int n = hal_dt_read(rx, sizeof(rx), 0);
        //     if (n > 0) {
        //         hal_dt_printf("[ray] host -> dev, %d bytes\r\n", n);
        //     }
        //     hal_dt_printf("[ray] alive, tick=%u, dropped=%u, kbd_led=0x%02x\r\n", (unsigned)tick++, (unsigned)hal_dt_dropped(), hal_kbd_get_leds());
        // }

        cnt++;

        static uint8_t btn_st_pre = !BOARD_BUTTON_PRESSED_VALUE;
        uint8_t btn_st = gpio_read_pin(BOARD_APP_GPIO_CTRL, BOARD_APP_GPIO_INDEX, BOARD_APP_GPIO_PIN);
        if (btn_st != btn_st_pre) {
            tx_thread_sleep(20);
            btn_st = gpio_read_pin(BOARD_APP_GPIO_CTRL, BOARD_APP_GPIO_INDEX, BOARD_APP_GPIO_PIN);
        }
        if (btn_st != btn_st_pre) {
            btn_st_pre = btn_st;
            if (btn_st == BOARD_BUTTON_PRESSED_VALUE) {
                // db_data.bif.gp_mode++;
                // db_data.bif.gp_mode %= HAL_GAMEPAD_MODE_MAX;
                
                g_obi.pi.test_cnt++;
                if (rotsta_success == db_update(DB_ITEM_TY_OBI, DB_SAVE_TY_NOW)) {
                    cst_obi_log("[obi] saved");
                    // tx_thread_sleep(100);
                    // disable_global_irq(CSR_MSTATUS_MIE_MASK);
                    // (*(volatile uint32_t *)(0xF40D4000UL + 8U)) = 0; // warning: this is rom reset flag,
                    // ppor_sw_reset(HPM_PPOR, 1000);
                    // while (1)
                    // {
                    // }
                }
                else {
                    hal_dt_printf("[obi] save failed\r\n");
                }

                hal_dt_printf("btn down (level=%u, gp_mode=%u)\r\n", (unsigned)btn_st, (unsigned)g_gamepad_mode);
            } 
            else {
                hal_dt_printf("btn up (level=%u)\r\n", (unsigned)btn_st);
                cst_obi_log("[obi] up");
                
            }
        }

        tx_thread_sleep(1);
    }
}


rotsta_t cst_init(void)
{
    rotsta_t ret = tx_thread_create(&cst_thread, "cst_thread", cst_thread_entry, 0U, cst_thread_stack, sizeof(cst_thread_stack), CST_THREAD_PRIORITY, CST_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ret != TX_SUCCESS) {
        printf(TAG "thread create failed, ret=%u\r\n", ret);
        return rotsta_fail;
    }
    return rotsta_success;
}


