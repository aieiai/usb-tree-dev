#include "tx_api.h"
#include "tx_config.h"

#include "hal_usb.h"
#include "hal_flash.h"

#include "db.h"

#include "keycore.h"
#include "keycalib.h"
#include "rgb.h"

#include "rotcc.h"
#include "rotcc_proto.h"
#include "upgrade.h"

#include "usb_config.h"

#include "board.h"
#include "hpm_ppor_drv.h"
#include "hpm_gpio_drv.h"

#define TAG "main: "

extern void webusb_hid_keyboard_init(uint8_t busid, uintptr_t reg_base);
extern void gamepad_init(uint8_t busid, uintptr_t reg_base);

/* Define what the initial system looks like.  */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    rotsta_t result = rotsta_success;

    result = db_init();
    if (result != rotsta_success) {
        printf(TAG "db init failed: 0x%08x\r\n", (unsigned)result);
    }

    result = keycal_init();
    if (result != rotsta_success) {
        printf(TAG "keycal init failed: 0x%08x, using defaults\r\n", (unsigned)result);
    }

    result = keycore_init();
    if (result != rotsta_success) {
        printf(TAG "keycore nit failed: 0x%08x, using defaults\r\n", (unsigned)result);
    }

    result = rgb_init();
    if (result != rotsta_success) {
        printf(TAG "rgb init failed: 0x%08x\r\n", (unsigned)result);
    }
    
    result = rotcc_init();
    if (result != rotsta_success) {
        printf(TAG "rotcc init failed: 0x%08x\r\n", (unsigned)result);
    }

    result = rotcc_proto_init();
    if (result != rotsta_success) {
        printf(TAG "rotcc proto init failed: 0x%08x\r\n", (unsigned)result);
    }

    hal_usb_init();
    // webusb_hid_keyboard_init(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE);
    // gamepad_init(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE);

}

int main(void)
{
    board_init();
    /* Enter the ThreadX kernel.  */
    tx_kernel_enter();
    for (;;) {
        ;
    }
    return 0;
}

