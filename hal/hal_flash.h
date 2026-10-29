#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdint.h>
#include <string.h>
#include "rot_state.h"



rot_state_t hal_flash_init(void);
rot_state_t hal_flash_read(uint32_t offset_addr, void *buf, uint32_t buf_size);
rot_state_t hal_flash_write(uint32_t offset_addr, const void *buf, uint32_t buf_size);

#endif
