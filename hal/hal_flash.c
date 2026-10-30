#include "hal_flash.h"

#include <stdbool.h>
#include <stdio.h>

#include "fdb_config.h"

#include "board.h"
#include "hpm_romapi.h"

#define _fencerw()  __asm volatile("fence rw, rw" ::: "memory")
#define _fenceim()  __asm volatile("fence.i" ::: "memory")

typedef struct {
    bool             ready;
    XPI_Type        *base;
    uint32_t         total_siz;
    uint32_t         page_siz;
    uint32_t         sector_siz;
    uint32_t         block_siz;
    uint32_t         param_offset;
    xpi_nor_config_t nor_cfg;
} hal_flash_drv_t;

static hal_flash_drv_t fl;


static bool hal_flash_in_range(uint32_t offset_addr, uint32_t buf_size)
{
    return (offset_addr < fl.total_siz) && (buf_size <= (fl.total_siz - offset_addr));
}

static rot_state_t hal_flash_check_writable(uint32_t offset_addr, uint32_t buf_size)
{
    if (!fl.ready) {
        return rot_state_flash_not_init;
    }
    if (buf_size == 0U) {
        return rot_state_invalid_argument;
    }
    if (!hal_flash_in_range(offset_addr, buf_size)) {
        return rot_state_flash_out_of_range;
    }

#if !HAL_FLASH_ALLOW_LOW_REGION
    // 参数区之前的区域归 XIP 固件使用，写/擦一律拒绝
    if (offset_addr < fl.param_offset) {
        return rot_state_flash_protected;
    }
#endif
    return rot_state_success;
}

uint32_t hal_flash_get_size(void)
{
    return fl.ready ? fl.total_siz : 0U;
}

uint32_t hal_flash_get_sector_size(void)
{
    return fl.ready ? fl.sector_siz : 0U;
}

uint32_t hal_flash_get_page_size(void)
{
    return fl.ready ? fl.page_siz : 0U;
}

uint32_t hal_flash_get_block_size(void)
{
    return fl.ready ? fl.block_siz : 0U;
}

uint32_t hal_flash_get_param_offset(void)
{
    return fl.ready ? fl.param_offset : 0U;
}

uint32_t hal_flash_get_param_size(void)
{
    return fl.ready ? FDB_END_ADDR : 0U;
}

const void *hal_flash_mapped(uint32_t offset_addr)
{
    if (!fl.ready || (offset_addr >= fl.total_siz)) {
        return NULL;
    }
    return (const void *)(uintptr_t)(BOARD_FLASH_BASE_ADDRESS + offset_addr);
}

rot_state_t hal_flash_read(uint32_t offset_addr, void *buf, uint32_t buf_size)
{
    if (!fl.ready) {
        return rot_state_flash_not_init;
    }
    if ((buf == NULL) || (buf_size == 0U)) {
        return rot_state_invalid_argument;
    }
    if (!hal_flash_in_range(offset_addr, buf_size)) {
        return rot_state_flash_out_of_range;
    }
    if (((offset_addr & 3U) != 0U) || ((buf_size & 3U) != 0U) || (((uintptr_t)buf & 3U) != 0U)) {
        return rot_state_flash_unaligned;
    }

    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    hpm_stat_t st = rom_xpi_nor_read(fl.base, xpi_xfer_channel_auto, &fl.nor_cfg, (uint32_t *)buf, offset_addr, buf_size);
    restore_global_irq(ctx);
    _fencerw();
    _fenceim();

    if (st != status_success) {
        return rot_state_flash_read_fail;
    }
    return rot_state_success;
}

rot_state_t hal_flash_write(uint32_t offset_addr, const void *buf, uint32_t buf_size)
{
    rot_state_t pre = hal_flash_check_writable(offset_addr, buf_size);
    if (pre != rot_state_success) {
        return pre;
    }
    if (buf == NULL) {
        return rot_state_invalid_argument;
    }
    if (((offset_addr & 3U) != 0U) || ((buf_size & 3U) != 0U) || (((uintptr_t)buf & 3U) != 0U)) {
        return rot_state_flash_unaligned;
    }

    const uint8_t *src = (const uint8_t *)buf;
    uint32_t left = buf_size;
    uint32_t off = offset_addr;

    /* 按 page 拆分：一次 XPI 编程指令最多写一个页 */
    while (left > 0U) {
        uint32_t in_page = fl.page_siz - (off % fl.page_siz);
        uint32_t chunk = (left < in_page) ? left : in_page;

        uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
        _fencerw();
        hpm_stat_t st = rom_xpi_nor_program(fl.base, xpi_xfer_channel_auto, &fl.nor_cfg, (const uint32_t *)src, off, chunk);
        restore_global_irq(ctx);
        _fencerw();
        _fenceim();

        if (st != status_success) {
            return rot_state_flash_program_fail;
        }

        off += chunk;
        src += chunk;
        left -= chunk;
    }

    return rot_state_success;
}

rot_state_t hal_flash_erase(uint32_t offset_addr, uint32_t buf_size)
{
    rot_state_t pre = hal_flash_check_writable(offset_addr, buf_size);
    if (pre != rot_state_success) {
        return pre;
    }
    if ((offset_addr % fl.sector_siz) != 0U || (buf_size % fl.sector_siz) != 0U) {
        return rot_state_flash_unaligned;
    }

    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    hpm_stat_t st = rom_xpi_nor_erase(fl.base, xpi_xfer_channel_auto, &fl.nor_cfg, offset_addr, buf_size);
    restore_global_irq(ctx);
    _fencerw();
    _fenceim();

    if (st != status_success) {
        return rot_state_flash_erase_fail;
    }
    return rot_state_success;
}

rot_state_t hal_flash_init(void)
{
    if (fl.ready) {
        return rot_state_success;
    }

    xpi_nor_config_option_t option;
    option.header.U  = BOARD_APP_XPI_NOR_CFG_OPT_HDR;
    option.option0.U = BOARD_APP_XPI_NOR_CFG_OPT_OPT0;
    option.option1.U = BOARD_APP_XPI_NOR_CFG_OPT_OPT1;
    fl.base = (XPI_Type *)BOARD_APP_XPI_NOR_XPI_BASE;

    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    hpm_stat_t st = rom_xpi_nor_auto_config(fl.base, &fl.nor_cfg, &option);
    restore_global_irq(ctx);
    // if ((ctx & CSR_MSTATUS_MIE_MASK) != 0U) {
    //     restore_global_irq(ctx);
    // }

    if (st != status_success) {
        return rot_state_fail;
    }

    _fencerw();
    _fenceim();

    (void)rom_xpi_nor_get_property(fl.base, &fl.nor_cfg, xpi_nor_property_total_size,  &fl.total_siz);
    (void)rom_xpi_nor_get_property(fl.base, &fl.nor_cfg, xpi_nor_property_page_size,   &fl.page_siz);
    (void)rom_xpi_nor_get_property(fl.base, &fl.nor_cfg, xpi_nor_property_sector_size, &fl.sector_siz);
    (void)rom_xpi_nor_get_property(fl.base, &fl.nor_cfg, xpi_nor_property_block_size,  &fl.block_siz);
    
    if ((fl.total_siz == 0U) || (fl.sector_siz == 0U) || (fl.page_siz == 0U) || (fl.total_siz < FDB_END_ADDR)) {
        return rot_state_fail;
    }

    fl.param_offset = fl.total_siz - FDB_END_ADDR;

    if (fl.total_siz != FDB_TOTAL_SIZ) {
        // target flash 与 config/FDB_confgig.h 规划不符，布局仍按实际容量自动推导
        printf("[flash] confirm: layout plan %uB, hardware %uB\r\n", (unsigned)FDB_TOTAL_SIZ, (unsigned)fl.total_siz);
    }

    fl.ready = true;
    return rot_state_success;
}
