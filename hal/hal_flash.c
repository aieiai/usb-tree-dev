#include "hal_flash.h"

#include <stdbool.h>
#include <stdio.h>

#include "fdb_map.h"

#include "board.h"
#include "hpm_romapi.h"
#include "hpm_l1c_drv.h"

#define TAG "hal_flash: "

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


#if (defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1))
    static hpm_mutex_handle_t flash_mutex_handle = NULL;
    #define FLASH_MUTEX_CREATE()                                  \
        if (flash_mutex_handle == NULL) {                         \
            flash_mutex_handle = hpm_mutex_create("flash_mutex"); \
        }
    #define FLASH_MUTEX_GET() hpm_mutex_get(flash_mutex_handle, HPM_WAIT_FOREVER);
    #define FLASH_MUTEX_PUT() hpm_mutex_put(flash_mutex_handle);
#elif (defined(CONFIG_THREADX) && (CONFIG_THREADX == 1))
    #include "tx_api.h"
    static TX_MUTEX flash_mutex;
    #define FLASH_MUTEX_CREATE()                                                         \
        if (TX_SUCCESS != tx_mutex_create(&flash_mutex, "flash_mutex", TX_NO_INHERIT)) { \
            printf("tx_mutex_create failed\n");                                          \
        }
    #define FLASH_MUTEX_GET() tx_mutex_get(&flash_mutex, TX_WAIT_FOREVER);
    #define FLASH_MUTEX_PUT() tx_mutex_put(&flash_mutex);
#else
    #define FLASH_MUTEX_CREATE()
    #define FLASH_MUTEX_GET()
    #define FLASH_MUTEX_PUT()
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

static hal_flash_msg_t hal_fm;

// flash 内容变化, 作废对应 XIP 区间的 D-Cache, 避免读到旧数据
static void hal_flash_invalidate_dcache(uint32_t offset_addr, uint32_t buf_size)
{
    if (l1c_dc_is_enabled() && (0u < buf_size)) {
        uint32_t flash_addr    = FDB_BASE_ADDR + offset_addr;
        uint32_t aligned_start = HPM_L1C_CACHELINE_ALIGN_DOWN((uint32_t)flash_addr);
        uint32_t aligned_end   = HPM_L1C_CACHELINE_ALIGN_UP((uint32_t)flash_addr + buf_size);
        uint32_t aligned_size  = aligned_end - aligned_start;
        l1c_dc_invalidate(aligned_start, aligned_size);
    }
}

static bool hal_flash_in_range(uint32_t offset_addr, uint32_t buf_size)
{
    return (offset_addr < hal_fm.total_siz) && (buf_size <= (hal_fm.total_siz - offset_addr));
}

static rot_state_t hal_flash_check_writable(uint32_t offset_addr, uint32_t buf_size)
{
    if (!hal_fm.ready) {
        return rot_state_flash_not_init;
    }
    if (buf_size == 0U) {
        return rot_state_invalid_argument;
    }
    if (!hal_flash_in_range(offset_addr, buf_size)) {
        return rot_state_flash_out_of_range;
    }

    return rot_state_success;
}

rot_state_t hal_flash_read_mem(uint32_t offset_addr, void *buf, uint32_t buf_size)
{
    hal_flash_invalidate_dcache(offset_addr, buf_size);
    memcpy(buf, (void *)(FDB_BASE_ADDR + offset_addr), buf_size);
    return rot_state_success;
}

rot_state_t hal_flash_read(uint32_t offset_addr, void *buf, uint32_t buf_size)
{
    if (!hal_fm.ready) {
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

    FLASH_MUTEX_GET();
    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    hpm_stat_t st = rom_xpi_nor_read(hal_fm.base, 
                                    xpi_xfer_channel_auto, 
                                    &hal_fm.nor_cfg, 
                                    (uint32_t *)buf, 
                                    offset_addr, 
                                    buf_size);
    _fencerw();
    _fenceim();
    restore_global_irq(ctx);
    FLASH_MUTEX_PUT();

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

    while (left > 0U) {
        uint32_t in_page = hal_fm.page_siz - (off % hal_fm.page_siz);
        uint32_t chunk = (left < in_page) ? left : in_page;

        FLASH_MUTEX_GET();
        uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
        _fencerw();
        hpm_stat_t st = rom_xpi_nor_program(hal_fm.base, 
                                            xpi_xfer_channel_auto, 
                                            &hal_fm.nor_cfg, 
                                            (const uint32_t *)src, 
                                            off, 
                                            chunk);
        _fencerw();
        _fenceim();
        restore_global_irq(ctx);
        FLASH_MUTEX_PUT();

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
    if ((offset_addr % hal_fm.sector_siz) != 0U || (buf_size % hal_fm.sector_siz) != 0U) {
        return rot_state_flash_unaligned;
    }

    FLASH_MUTEX_GET();
    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    hpm_stat_t st = rom_xpi_nor_erase(hal_fm.base, 
                                    xpi_xfer_channel_auto, 
                                    &hal_fm.nor_cfg, 
                                    offset_addr, 
                                    buf_size);
    _fencerw();
    _fenceim();
    restore_global_irq(ctx);
    FLASH_MUTEX_PUT();

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

const void *hal_flash_get_mapped(uint32_t offset_addr)
{
    if (!hal_fm.ready || (offset_addr >= hal_fm.total_siz)) {
        return NULL;
    }
    return (const void *)(uintptr_t)(FDB_BASE_ADDR + offset_addr);
}

rot_state_t hal_flash_init(void)
{
    if (hal_fm.ready) {
        return rot_state_success;
    }

    memset(&hal_fm, 0, sizeof(hal_fm));

    hpm_stat_t st = status_success;
    xpi_nor_config_option_t option;

    option.header.U  = BOARD_APP_XPI_NOR_CFG_OPT_HDR;
    option.option0.U = BOARD_APP_XPI_NOR_CFG_OPT_OPT0;
    option.option1.U = BOARD_APP_XPI_NOR_CFG_OPT_OPT1;
    hal_fm.base = (XPI_Type *)BOARD_APP_XPI_NOR_XPI_BASE;

    uint32_t ctx = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    _fencerw();
    st = rom_xpi_nor_auto_config(hal_fm.base, &hal_fm.nor_cfg, &option);
    _fencerw();
    _fenceim();
    restore_global_irq(ctx);

    if (st != status_success) {
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
        (hal_fm.sector_siz > hal_fm.block_siz) ||
        (hal_fm.total_siz < (FDB_DATA_END_ADDR - FDB_BASE_ADDR)) ||
        (hal_fm.total_siz > (UINT32_MAX - BOARD_FLASH_BASE_ADDRESS))) {
        return rot_state_fail;
    }

    if (hal_fm.total_siz != FDB_TOTAL_SIZ) {
        // target flash 与 config/fdb_map.h 规划不符，布局仍按编译期固定地址使用
        printf(TAG "total size mismatch %d != %d, use fixed address\r\n", hal_fm.total_siz, FDB_TOTAL_SIZ);
    }

    FLASH_MUTEX_CREATE();
    hal_fm.ready = true;
    return rot_state_success;
}
