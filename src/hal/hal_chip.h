#ifndef HAL_CHIP_H
#define HAL_CHIP_H

#include <stdint.h>

#include "rotsta.h"


/* HPM OTP table: chip id at byte offset 64, i.e. word 16. */
#define CHIP_OTP_ID_WORD (16U)


typedef struct {
    uint32_t otp_chip_id;
    uint32_t flash_size;
} hal_chip_info_t;


/* Returns the build time SoC identity and the OTP chip id. */
rotsta_t hal_chip_get_info(hal_chip_info_t *info);


#endif
