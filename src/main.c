#include "tx_api.h"
#include "tx_config.h"

#include "hal_usb.h"
#include "hal_flash.h"
#include "hal_chip.h"

#include "cst.h"
#include "db.h"

#include "keycore.h"
#include "rgb.h"

#include "rotcc.h"
#include "rotcc_proto.h"
#include "upgrade.h"

#include "usb_config.h"

#include "board.h"
#include "hpm_ppor_drv.h"
#include "hpm_gpio_drv.h"


#define TAG "main: "
#define LED_FLASH_PERIOD_IN_MS  (300)

/* 手柄演示：1 = 启动即以 Xbox 模式枚举，并创建一个 4ms 周期的手柄上报任务。
 * 该任务做逐位自检：每个按键 / 方向 / 摇杆轴 / 扳机依次单独动作一遍，
 * 一轮 27 项 ≈ 27 秒，循环播放；当前项通过 DT 通道打印。
 * 接入真实输入后删掉这个任务即可。 */
#ifndef APP_GAMEPAD_DEMO
#define APP_GAMEPAD_DEMO      0
#endif




extern void webusb_hid_keyboard_init(uint8_t busid, uintptr_t reg_base);
extern void gamepad_init(uint8_t busid, uintptr_t reg_base);
extern void gamepad_test(uint8_t busid);

/* Define what the initial system looks like.  */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    rotsta_t state;
    hal_chip_info_t chip;

    state = db_init();
    if (state != rotsta_success) {
        printf(TAG "db init failed: 0x%08x\r\n", (unsigned)state);
    }

    state = keycore_init();
    if (state != rotsta_success) {
        printf(TAG "keycore nit failed: 0x%08x, using defaults\r\n", (unsigned)state);
    }


    if (hal_chip_get_info(&chip) == rotsta_success) {
        printf(TAG "OTP chip id 0x%08x, flash %u KB\r\n", (unsigned)chip.otp_chip_id, (unsigned)(chip.flash_size / 1024U));
    }

    state = upgrade_init();
    if (state != rotsta_success) {
        printf(TAG "upgrade init failed: 0x%08x\r\n", (unsigned)state);
    }

    state = rotcc_init();
    if (state != rotsta_success) {
        printf(TAG "rotcc init failed: 0x%08x\r\n", (unsigned)state);
    }

    state = rgb_init();
    if (state != rotsta_success) {
        printf(TAG "rgb init failed: 0x%08x\r\n", (unsigned)state);
    }
    
    state = rotcc_proto_init();
    if (state != rotsta_success) {
        printf(TAG "rotcc proto init failed: 0x%08x\r\n", (unsigned)state);
    }

    cst_init();

    hal_usb_init();
    // webusb_hid_keyboard_init(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE);
    // gamepad_init(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE);

}

int main(void)
{
    board_init();
    // board_init_led_pins();
    // board_init_gpio_pins();
    // gpio_set_pin_input(BOARD_APP_GPIO_CTRL, BOARD_APP_GPIO_INDEX, BOARD_APP_GPIO_PIN);
    printf("cherry usb threadx hid_keyboard device sample.\n");
    /* Enter the ThreadX kernel.  */
    tx_kernel_enter();
    for (;;) {
        ;
    }
    return 0;
}

