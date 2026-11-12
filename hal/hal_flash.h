#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "rotsta.h"


enum {
    rotsta_flash_program_fail = ROTSTA_MAKE(rotsta_group_flash, 1),
    rotsta_flash_erase_fail,
    rotsta_flash_verify_fail,
    rotsta_flash_out_of_range,
    rotsta_flash_protected,
    rotsta_flash_unaligned,
    rotsta_flash_not_init,
    rotsta_flash_read_fail,
    rotsta_flash_busy,
};

rotsta_t hal_flash_init(void);
uint32_t hal_flash_get_size(void);
uint32_t hal_flash_get_sector_size(void);
uint32_t hal_flash_get_page_size(void);
uint32_t hal_flash_get_block_size(void);

rotsta_t hal_flash_read(uint32_t offset_addr, void *buf, uint32_t buf_size);
// rotsta_t hal_flash_read_mem(uint32_t offset_addr, void *buf, uint32_t buf_size);
rotsta_t hal_flash_compare(uint32_t offset_addr, const void *buf, uint32_t buf_size, bool *equal);
rotsta_t hal_flash_write(uint32_t offset_addr, const void *buf, uint32_t buf_size);
rotsta_t hal_flash_erase(uint32_t offset_addr, uint32_t buf_size);

// Return the application slot selected by the boot ROM. 0: remap disabled (APP1), 1: remap enabled (APP2).
rotsta_t hal_flash_get_active_app_slot(uint8_t *slot);

#endif
