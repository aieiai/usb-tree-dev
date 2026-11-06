#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "rot_state.h"


enum {
    rot_state_flash_program_fail = ROT_STATE_MAKE(rot_state_group_flash, 1),
    rot_state_flash_erase_fail,
    rot_state_flash_verify_fail,
    rot_state_flash_out_of_range,
    rot_state_flash_protected,
    rot_state_flash_unaligned,
    rot_state_flash_not_init,
    rot_state_flash_read_fail,
    rot_state_flash_busy,
};

rot_state_t hal_flash_init(void);
uint32_t hal_flash_get_size(void);
uint32_t hal_flash_get_sector_size(void);
uint32_t hal_flash_get_page_size(void);
uint32_t hal_flash_get_block_size(void);

rot_state_t hal_flash_read(uint32_t offset_addr, void *buf, uint32_t buf_size);
// rot_state_t hal_flash_read_mem(uint32_t offset_addr, void *buf, uint32_t buf_size);
rot_state_t hal_flash_compare(uint32_t offset_addr, const void *buf, uint32_t buf_size, bool *equal);
rot_state_t hal_flash_write(uint32_t offset_addr, const void *buf, uint32_t buf_size);
rot_state_t hal_flash_erase(uint32_t offset_addr, uint32_t buf_size);

// Return the application slot selected by the boot ROM. 0: remap disabled (APP1), 1: remap enabled (APP2).
rot_state_t hal_flash_get_active_app_slot(uint8_t *slot);

#endif
