#ifndef OTA_API_H
#define OTA_API_H

#include <stdbool.h>
#include <stdint.h>

#include "rot_state.h"
#include "upgrade.h"


#define USER_UPGREAD_FLAG_MAGIC UPGRADE_IMAGE_HEADER_MAGIC
#define BOARD_DEVICE_ID         UPGRADE_DEVICE_ID


void ota_board_flash_init(void);
bool ota_board_auto_write(const void *src, uint32_t len);
void ota_board_complete_reset(void);
uint8_t ota_check_current_otaindex(void);
rot_state_t ota_board_last_state(void);


#endif
