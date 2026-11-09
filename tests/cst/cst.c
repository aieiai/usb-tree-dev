#include "cst.h"

#include "hal_usb.h"


#include "fdb.h"

#include "tx_api.h"
#include "tx_config.h"

#include "board.h"
#include "hpm_ppor_drv.h"
#include "hpm_gpio_drv.h"


#define TAG "cst"

static TX_THREAD thread_cst;
static uint8_t thread_cst_stack[CST_TASK_STACK] __attribute__((aligned(8U)));

static void cst_task(ULONG id)
{
    (void)id;

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
                // fdb_data.bif.gp_mode++;
                // fdb_data.bif.gp_mode %= HAL_GAMEPAD_MODE_MAX;
                
                if (rot_state_success == fdb_update(FDB_ITEM_TY_SOBI, FDB_SAVE_TY_NOW)) {
                    tx_thread_sleep(100);
                    disable_global_irq(CSR_MSTATUS_MIE_MASK);
                    (*(volatile uint32_t *)(0xF40D4000UL + 8U)) = 0; // warning: this is rom reset flag,
                    ppor_sw_reset(HPM_PPOR, 1000);
                    while (1)
                    {
                    }
                }

                hal_dt_printf("btn down (level=%u, gp_mode=%u)\r\n", (unsigned)btn_st, (unsigned)g_gamepad_mode);
            } 
            else {
                hal_dt_printf("btn up (level=%u)\r\n", (unsigned)btn_st);
            }
        }

        tx_thread_sleep(1);
    }
}


rot_state_t cst_init(void)
{
    rot_state_t ret = tx_thread_create(&thread_cst, "thread_cst", cst_task, 0U, thread_cst_stack, sizeof(thread_cst_stack), CST_TASK_PRIORITY, CST_TASK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ret != TX_SUCCESS) {
        printf(TAG "thread create failed, ret=%u\r\n", ret);
        return rot_state_fail;
    }
    return rot_state_success;
}


