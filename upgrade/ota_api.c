#include "ota_api.h"

#include <stdio.h>


#define TAG "ota: "

static rot_state_t s_last_state = rot_state_success;


void ota_board_flash_init(void)
{
    s_last_state = upgrade_init();
    if (s_last_state != rot_state_success) {
        printf(TAG "flash init failed: 0x%08x\r\n", (unsigned)s_last_state);
    }
}

bool ota_board_auto_write(const void *src, uint32_t len)
{
    bool complete = false;

    s_last_state = upgrade_feed(src, len, &complete);
    if (s_last_state != rot_state_success) {
        printf(TAG "image write failed: 0x%08x\r\n", (unsigned)s_last_state);
        return false;
    }
    return complete;
}

void ota_board_complete_reset(void)
{
    s_last_state = upgrade_finish();
    if (s_last_state != rot_state_success) {
        printf(TAG "image verification failed: 0x%08x\r\n", (unsigned)s_last_state);
        return;
    }
    upgrade_reboot();
}

uint8_t ota_check_current_otaindex(void)
{
    uint8_t slot = 0xffU;
    rot_state_t state = upgrade_get_active_slot(&slot);

    if (state != rot_state_success) {
        printf(TAG "active slot query failed: 0x%08x\r\n", (unsigned)state);
        return 0xffU;
    }
    return slot;
}

rot_state_t ota_board_last_state(void)
{
    return s_last_state;
}
