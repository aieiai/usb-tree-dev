#include "hal_chip.h"

#include <stddef.h>

#include "flash_layout.h"
#include "hpm_romapi.h"


rotsta_t hal_chip_get_info(hal_chip_info_t *info)
{
    if (info == NULL) {
        return rotsta_invalid_argument;
    }

    info->otp_chip_id = rom_otp_read_from_shadow(CHIP_OTP_ID_WORD);
    info->flash_size = FLASH_TOTAL_SIZ;
    return rotsta_success;
}
