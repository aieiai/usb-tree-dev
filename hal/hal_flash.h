#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdint.h>
#include <string.h>
#include "rot_state.h"

// 放开参数区以外的写/擦 (DFU 升级等特殊场景才允许)
#ifndef HAL_FLASH_ALLOW_LOW_REGION
#define HAL_FLASH_ALLOW_LOW_REGION   0
#endif

enum {
    rot_state_flash_program_fail = ROT_STATE_MAKE(rot_state_group_flash, 1),
    rot_state_flash_erase_fail,
    rot_state_flash_verify_fail,
    rot_state_flash_out_of_range,
    rot_state_flash_protected,
    rot_state_flash_unaligned,
    rot_state_flash_not_init,
    rot_state_flash_read_fail,
};

rot_state_t hal_flash_init(void);
uint32_t hal_flash_get_size(void);
uint32_t hal_flash_get_sector_size(void);
uint32_t hal_flash_get_page_size(void);
uint32_t hal_flash_get_block_size(void);
uint32_t hal_flash_get_param_offset(void);
uint32_t hal_flash_get_param_size(void);

rot_state_t hal_flash_read(uint32_t offset_addr, void *buf, uint32_t buf_size);
rot_state_t hal_flash_write(uint32_t offset_addr, const void *buf, uint32_t buf_size);
rot_state_t hal_flash_erase(uint32_t offset_addr, uint32_t buf_size);
const void *hal_flash_mapped(uint32_t offset_addr);

#endif
