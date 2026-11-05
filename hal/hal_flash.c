#include "hal_flash.h"
#include "fdb_map.h"
#include "rot_utils.h"

#include "board.h"
#include "hpm_romapi.h"
#include "hpm_l1c_drv.h"

#define TAG "hal_flash: "
#define NOR_XPI_BASE  BOARD_APP_XPI_NOR_XPI_BASE

#ifndef HAL_FLASH_WRITE_BUFFER_SIZE
#define HAL_FLASH_WRITE_BUFFER_SIZE FDB_PAGE_SIZ
#endif

#if defined(__riscv)
    #define _fencerw()  __asm volatile("fence rw, rw" ::: "memory")
    #define _fenceim()  __asm volatile("fence.i" ::: "memory")
#elif defined(__arm__)
    #define _fencerw()  __asm volatile("" ::: "memory")
    #define _fenceim()  __asm volatile("" ::: "memory")
#else
    #define _fencerw()  __asm volatile("" ::: "memory")
    #define _fenceim()  __asm volatile("" ::: "memory")
#endif


_Static_assert(FDB_BASE_ADDR == BOARD_FLASH_BASE_ADDRESS, "FDB_BASE_ADDR must match BOARD_FLASH_BASE_ADDRESS");


typedef struct {
    bool             ready;
    XPI_Type        *base;
    uint32_t         total_siz;
    uint32_t         page_siz;
    uint32_t         sector_siz;
    uint32_t         block_siz;
    xpi_nor_config_t nor_cfg;
} hal_flash_msg_t;


static bool mutex_locked = false;
static uint8_t _write_buffer[HAL_FLASH_WRITE_BUFFER_SIZE] __attribute__((aligned(4)));
static hal_flash_msg_t hal_fm = {0u};

ROT_ATTR_USED
static bool _mutex_get(void)
{
    uint32_t irq_state = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    bool acquired = false;
    if (!mutex_locked) {
        mutex_locked = true;
        acquired = true;
    }
    restore_global_irq(irq_state);
    return acquired;
}

ROT_ATTR_USED
static bool _mutex_put(void)
{
    uint32_t irq_state = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    mutex_locked = false;
    restore_global_irq(irq_state);
    return true;
}

// flash 内容变化, 作废对应 XIP 区间的 D-Cache, 避免读到旧数据
static void _flash_invalidate_dcache(uint32_t offset_addr, uint32_t buf_size)
{
    if (l1c_dc_is_enabled() && (0u < buf_size)) {
        uint32_t flash_addr    = FDB_BASE_ADDR + offset_addr;
        uint32_t aligned_start = HPM_L1C_CACHELINE_ALIGN_DOWN((uint32_t)flash_addr);
        uint32_t aligned_end   = HPM_L1C_CACHELINE_ALIGN_UP((uint32_t)flash_addr + buf_size);
        uint32_t aligned_size  = aligned_end - aligned_start;
        l1c_dc_invalidate(aligned_start, aligned_size);
    }
}

static bool _flash_in_range(uint32_t offset_addr, uint32_t buf_size)
{
    return (offset_addr < hal_fm.total_siz) && (buf_size <= (hal_fm.total_siz - offset_addr));
}

static rot_state_t _flash_check_range(uint32_t offset_addr, uint32_t buf_size)
{
    if (!hal_fm.ready) {
        return rot_state_flash_not_init;
    }
    if (buf_size == 0U) {
        return rot_state_invalid_argument;
    }
    if (!_flash_in_range(offset_addr, buf_size)) {
        return rot_state_flash_out_of_range;
    }

    return rot_state_success;
}

__attribute__ ((unused))
static const void *_flash_get_mapped(uint32_t offset_addr)
{
    if (!hal_fm.ready || (offset_addr >= hal_fm.total_siz)) {
        return NULL;
    }
    return (const void *)(uintptr_t)(FDB_BASE_ADDR + offset_addr);
}

static rot_state_t _flash_program_page(uint32_t offset_addr, const void *buf, uint32_t buf_size)
{
    uint32_t ctx, page_offset = offset_addr % hal_fm.page_siz;

    if ((buf_size == 0U) ||
        (buf_size > hal_fm.page_siz) ||
        (buf_size > sizeof(_write_buffer)) ||
        (buf_size > (hal_fm.page_siz - page_offset))) {
        return rot_state_flash_unaligned;
    }

    ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    memcpy(_write_buffer, buf, buf_size);
    _fencerw();
    hpm_stat_t st = rom_xpi_nor_program(hal_fm.base, xpi_xfer_channel_auto, &hal_fm.nor_cfg, (const uint32_t *)_write_buffer, offset_addr, buf_size);
    _fencerw();
    _fenceim();
    restore_global_irq(ctx);

    if (st != status_success) {
        printf(TAG "program failed at 0x%08x, size %u, st = %d\r\n", (unsigned)offset_addr, (unsigned)buf_size, (int)st);
        return rot_state_flash_program_fail;
    }

    return rot_state_success;
}

static hpm_stat_t _flash_erase_region(uint32_t offset_addr, uint32_t buf_size)
{
    hpm_stat_t st          = status_success;
    uint32_t erase_addr    = offset_addr;
    uint32_t remaining     = buf_size;
    bool block_erase_valid = true;

    while (remaining > 0U) {
        uint32_t erase_size = hal_fm.sector_siz;

        if (block_erase_valid && (remaining >= hal_fm.block_siz) && ((erase_addr % hal_fm.block_siz) == 0U)) {
            st = rom_xpi_nor_erase_block(hal_fm.base, xpi_xfer_channel_auto, &hal_fm.nor_cfg, erase_addr);
            if (st == status_success) {
                erase_size = hal_fm.block_siz;
            } else {
                printf(TAG "block erase failed at 0x%08x, retry by sector, st = %d\r\n",
                       (unsigned)erase_addr, (int)st);
                block_erase_valid = false;
            }
        }

        if (erase_size == hal_fm.sector_siz) {
            st = rom_xpi_nor_erase_sector(hal_fm.base, xpi_xfer_channel_auto, &hal_fm.nor_cfg, erase_addr);
        }

        if (st != status_success) {
            printf(TAG "erase failed at 0x%08x, size %u, st = %d\r\n", (unsigned)erase_addr, (unsigned)erase_size, (int)st);
            return st;
        }

        erase_addr += erase_size;
        remaining -= erase_size;
    }
    
    return st;
}

rot_state_t hal_flash_read_mem(uint32_t offset_addr, void *buf, uint32_t buf_size)
{
    rot_state_t pre = _flash_check_range(offset_addr, buf_size);
    if (pre != rot_state_success) {
        return pre;
    }
    if (buf == NULL) {
        return rot_state_invalid_argument;
    }

    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _flash_invalidate_dcache(offset_addr, buf_size);
    memcpy(buf, (void *)(FDB_BASE_ADDR + offset_addr), buf_size);
    restore_global_irq(ctx);

    return rot_state_success;
}

rot_state_t hal_flash_read(uint32_t offset_addr, void *buf, uint32_t buf_size)
{
    rot_state_t pre = _flash_check_range(offset_addr, buf_size);
    if (pre != rot_state_success) {
        return pre;
    }
    if (buf == NULL) {
        return rot_state_invalid_argument;
    }
    if (((offset_addr & 3U) != 0U) || ((buf_size & 3U) != 0U) || (((uintptr_t)buf & 3U) != 0U)) {
        return rot_state_flash_unaligned;
    }

    _flash_invalidate_dcache(offset_addr, buf_size);

    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    hpm_stat_t st = rom_xpi_nor_read(hal_fm.base, xpi_xfer_channel_auto, &hal_fm.nor_cfg, (uint32_t *)buf, offset_addr, buf_size);
    _fencerw();
    _fenceim();
    restore_global_irq(ctx);

    if (st != status_success) {
        printf(TAG "read failed at 0x%08x, size %u, st = %d\r\n",
               (unsigned)offset_addr, (unsigned)buf_size, (int)st);
        return rot_state_flash_read_fail;
    }
    return rot_state_success;
}

rot_state_t hal_flash_compare(uint32_t offset_addr, const void *buf, uint32_t buf_size, bool *equal)
{
    rot_state_t pre = _flash_check_range(offset_addr, buf_size);
    if (pre != rot_state_success) {
        return pre;
    }
    if ((buf == NULL) || (equal == NULL)) {
        return rot_state_invalid_argument;
    }
    if (((offset_addr & 3U) != 0U) || ((buf_size & 3U) != 0U)) {
        return rot_state_flash_unaligned;
    }

    const uint8_t *src = (const uint8_t *)buf;
    uint32_t off = offset_addr;
    uint32_t left = buf_size;
    bool is_equal = true;
    hpm_stat_t st = status_success;
    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);

    _fencerw();
    while (left > 0U) {
        uint32_t page_offset = off % hal_fm.page_siz;
        uint32_t bytes_to_page_end = hal_fm.page_siz - page_offset;
        uint32_t compare_size = (left < bytes_to_page_end) ? left : bytes_to_page_end;
        uint32_t compare_addr = off;

        st = rom_xpi_nor_read(hal_fm.base,
                              xpi_xfer_channel_auto,
                              &hal_fm.nor_cfg,
                              (uint32_t *)_write_buffer,
                              compare_addr,
                              compare_size);
        if (st != status_success) {
            break;
        }
        if (memcmp(_write_buffer, src, compare_size) != 0) {
            is_equal = false;
            break;
        }

        off += compare_size;
        src += compare_size;
        left -= compare_size;
    }
    _fencerw();
    _fenceim();
    restore_global_irq(ctx);

    if (st != status_success) {
        return rot_state_flash_read_fail;
    }
    *equal = is_equal && (left == 0U);
    return rot_state_success;
}

rot_state_t hal_flash_write(uint32_t offset_addr, const void *buf, uint32_t buf_size)
{
    rot_state_t pre = _flash_check_range(offset_addr, buf_size);
    if (pre != rot_state_success) {
        return pre;
    }
    if (buf == NULL) {
        return rot_state_invalid_argument;
    }
    if (((offset_addr & 3U) != 0U) || ((buf_size & 3U) != 0U)) {
        return rot_state_flash_unaligned;
    }

    const uint8_t *src = (const uint8_t *)buf;
    uint32_t left = buf_size;
    uint32_t off = offset_addr;
    rot_state_t ret = rot_state_success;

    // Split one logical write into explicit page-sized ROM program calls.
    while (left > 0U) {
        uint32_t page_offset = off % hal_fm.page_siz;
        uint32_t bytes_to_page_end = hal_fm.page_siz - page_offset;
        uint32_t page_write_size = (left < bytes_to_page_end) ? left : bytes_to_page_end;

        ret = _flash_program_page(off, src, page_write_size);
        if (ret != rot_state_success) {
            break;
        }

        off += page_write_size;
        src += page_write_size;
        left -= page_write_size;
    }

    _flash_invalidate_dcache(offset_addr, buf_size);
    return ret;
}

rot_state_t hal_flash_erase(uint32_t offset_addr, uint32_t buf_size)
{
    hpm_stat_t st;
    rot_state_t pre = _flash_check_range(offset_addr, buf_size);
    if (pre != rot_state_success) {
        return pre;
    }
    if ((offset_addr % hal_fm.sector_siz) != 0U || (buf_size % hal_fm.sector_siz) != 0U) {
        return rot_state_flash_unaligned;
    }

    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    st = _flash_erase_region(offset_addr, buf_size);
    _fencerw();
    _fenceim();
    restore_global_irq(ctx);

    _flash_invalidate_dcache(offset_addr, buf_size);

    if (st != status_success) {
        return rot_state_flash_erase_fail;
    }
    return rot_state_success;
}

uint32_t hal_flash_get_size(void)
{
    return hal_fm.ready ? hal_fm.total_siz : 0U;
}

uint32_t hal_flash_get_sector_size(void)
{
    return hal_fm.ready ? hal_fm.sector_siz : 0U;
}

uint32_t hal_flash_get_page_size(void)
{
    return hal_fm.ready ? hal_fm.page_siz : 0U;
}

uint32_t hal_flash_get_block_size(void)
{
    return hal_fm.ready ? hal_fm.block_siz : 0U;
}

rot_state_t hal_flash_init(void)
{
    if (hal_fm.ready) {
        return rot_state_success;
    }
    hpm_stat_t st = status_success;
    xpi_nor_config_option_t option;

    memset(&hal_fm, 0, sizeof(hal_fm));
    hal_fm.base = (XPI_Type *)NOR_XPI_BASE;

    option.header.U  = BOARD_APP_XPI_NOR_CFG_OPT_HDR;
    option.option0.U = BOARD_APP_XPI_NOR_CFG_OPT_OPT0;
    option.option1.U = BOARD_APP_XPI_NOR_CFG_OPT_OPT1;

    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    st = rom_xpi_nor_auto_config(hal_fm.base, &hal_fm.nor_cfg, &option);
    _fencerw();
    _fenceim();
    restore_global_irq(ctx);

    if (st != status_success) {
        printf(TAG "xpi nor auto config failed, st = %d\r\n", st);
        return rot_state_fail;
    }
    st = rom_xpi_nor_get_property(hal_fm.base, &hal_fm.nor_cfg, xpi_nor_property_total_size, &hal_fm.total_siz);
    if (st != status_success) {
        printf(TAG "get total size failed, st = %d\r\n", st);
    }
    st = rom_xpi_nor_get_property(hal_fm.base, &hal_fm.nor_cfg, xpi_nor_property_page_size, &hal_fm.page_siz);
    if (st != status_success) {
        printf(TAG "get page size failed, st = %d\r\n", st);
    }
    st = rom_xpi_nor_get_property(hal_fm.base, &hal_fm.nor_cfg, xpi_nor_property_sector_size, &hal_fm.sector_siz);
    if (st != status_success) {
        printf(TAG "get sector size failed, st = %d\r\n", st);
    }
    st = rom_xpi_nor_get_property(hal_fm.base, &hal_fm.nor_cfg, xpi_nor_property_block_size, &hal_fm.block_siz);
    if (st != status_success) {
        printf(TAG "get block size failed, st = %d\r\n", st);
    }

    if ((hal_fm.total_siz == 0U) ||
        (hal_fm.page_siz == 0U) ||
        (hal_fm.sector_siz == 0U) ||
        (hal_fm.block_siz == 0U) ||
        (hal_fm.page_siz > hal_fm.sector_siz) ||
        (hal_fm.page_siz > sizeof(_write_buffer)) ||
        ((hal_fm.page_siz & 3U) != 0U) ||
        (hal_fm.sector_siz > hal_fm.block_siz) ||
        ((hal_fm.block_siz % hal_fm.sector_siz) != 0U) ||
        (hal_fm.total_siz < (FDB_DATA_END_ADDR - FDB_BASE_ADDR)) ||
        (hal_fm.total_siz > (UINT32_MAX - BOARD_FLASH_BASE_ADDRESS))) {
        printf(TAG "size mismatch\r\n");
        return rot_state_fail;
    }

    if (hal_fm.total_siz != FDB_TOTAL_SIZ) {
        // target flash 与 config/fdb_map.h 规划不符，布局仍按编译期固定地址使用
        printf(TAG "total size mismatch %d != %d, use fixed address\r\n", hal_fm.total_siz, FDB_TOTAL_SIZ);
    }

    printf(TAG "chip %u KB, block %u KB, sector %u KB, page %u B\r\n", hal_fm.total_siz / 1024u, hal_fm.block_siz / 1024u, hal_fm.sector_siz / 1024u, hal_fm.page_siz);
    hal_fm.ready = true;
    return rot_state_success;
}
