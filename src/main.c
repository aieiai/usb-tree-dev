
#include "tx_api.h"
#include "tx_task_config.h"

#include "hal_usb.h"
#include "hal_kbd.h"
#include "hal_dt.h"
#include "hal_lighting.h"

#include "usb_osal.h"
#include "usb_config.h"

#include "board.h"
#include "hpm_gpio_drv.h"


#define task1_PRIORITY          (TX_MAX_PRIORITIES - 5U)
#define TX_MEM_POOL_SIZE        (16U * 1024U)
#define LED_FLASH_PERIOD_IN_MS  (300)

/* 演示/自检日志：量产时改为 0 或直接删掉演示代码，DT 接口本身不受影响 */
#ifndef APP_LOG_DEMO
#define APP_LOG_DEMO           1
#endif


TX_BYTE_POOL tx_byte_pool;
static UCHAR tx_memory_area[TX_MEM_POOL_SIZE] __attribute__((aligned(8)));

#if APP_LOG_DEMO
/* 演示用：把 Windows 下发的第 0 颗灯颜色记下来，在诊断数据里显示，便于确认灯效数据到了 */
static volatile uint8_t s_lamp0[4];

void hal_lighting_set_color(uint16_t lamp_id, uint8_t red, uint8_t green, uint8_t blue, uint8_t intensity)
{
    if (lamp_id == 0U) {
        s_lamp0[0] = red;
        s_lamp0[1] = green;
        s_lamp0[2] = blue;
        s_lamp0[3] = intensity;
    }
}
#endif

static void task1(void *pvParameters)
{
    (void)pvParameters;
    uint32_t tick = 0;
    uint8_t rx[64];

#if APP_LOG_DEMO
    printf("[cherryusb hid keyboard with freertos sample]: task started.\n");
    hal_dt_puts("[ray] diagnosis tracking channel ready\r\n");
#endif

    while (1) {
#if APP_LOG_DEMO
        int n;

        /* DT OUT 端点：上位机下发的数据，这里回显一下，方便验证双向通道 */
        n = hal_dt_read(rx, sizeof(rx), 0);
        if (n > 0) {
            hal_dt_printf("[ray] host -> dev, %d bytes\r\n", n);
        }

        /* kbd_led 反映键盘 LED 输出报告；lamp0 反映 Windows 下发的灯效颜色 */
        hal_dt_printf("[ray] alive, tick=%u, dropped=%u, kbd_led=0x%02x, lamp0=%02x%02x%02x\r\n",
                      (unsigned)tick++, (unsigned)hal_dt_dropped(), hal_kbd_get_leds(),
                      s_lamp0[0], s_lamp0[1], s_lamp0[2]);
#else
        (void)tick;
        (void)rx;
#endif
        usb_osal_msleep(1000);
    }
}

extern void webusb_hid_keyboard_init(uint8_t busid, uintptr_t reg_base);
/* Define what the initial system looks like.  */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    
    tx_byte_pool_create(&tx_byte_pool, "tx byte pool", tx_memory_area, TX_MEM_POOL_SIZE);

    hal_usb_init();
    // webusb_hid_keyboard_init(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE);


    if (usb_osal_thread_create("task1", 8192U, task1_PRIORITY, task1, NULL) == NULL) {
        perror("Task1 creation failed!.\n");
        for (;;) {
            ;
        }
    }
}
int main(void)
{
    board_init();
    board_init_led_pins();
    board_init_gpio_pins();
    gpio_set_pin_input(BOARD_APP_GPIO_CTRL, BOARD_APP_GPIO_INDEX, BOARD_APP_GPIO_PIN);

    board_timer_create(LED_FLASH_PERIOD_IN_MS, board_led_toggle);

    board_init_usb((USB_Type *)CONFIG_USBD_REG_BASE);
    intc_set_irq_priority(CONFIG_HPM_USBD_IRQn, 2);

    printf("cherry usb threadx hid_keyboard device sample.\n");

    /* Enter the ThreadX kernel.  */
    tx_kernel_enter();

    for (;;) {
        ;
    }
    return 0;
}

