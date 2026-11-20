#include "hal_spi.h"
#include "board.h"

#include "hpm_csr_drv.h"
#include "hpm_clock_drv.h"
#include "hpm_spi_drv.h"
#include "hpm_dmav2_drv.h"
#include "hpm_dmamux_drv.h"
#include "hpm_interrupt.h"

/* 应用 SPI：SPI3，数据线 MOSI 在 PC13 */
#define HAL_SPI_BASE               BOARD_APP_SPI_BASE
#define HAL_SPI_CLOCK_NAME         BOARD_APP_SPI_CLK_NAME
#define HAL_SPI_TX_DMA             BOARD_APP_SPI_TX_DMA
#define HAL_SPI_DMA_CONTROLLER     BOARD_APP_DMA0
#define HAL_SPI_DMAMUX_CONTROLLER  BOARD_APP_DMAMUX
#define HAL_SPI_DMA_CHANNEL        (8U)
#define HAL_SPI_DMA_MUX_CHANNEL    DMA_SOC_CHN_TO_DMAMUX_CHN(HAL_SPI_DMA_CONTROLLER, HAL_SPI_DMA_CHANNEL)

#define HAL_SPI_DATA_BITS          (8U)
#define HAL_SPI_POLL_INTERVAL_US   (10U)
#define HAL_SPI_TX_TIMEOUT_US      (100000UL)
/* 两次发送起始时刻的最小间隔，约 60fps */
#define HAL_SPI_TX_GAP_US          (16000UL)

static bool s_inited;
static uint64_t s_tx_start_cycle;
static uint64_t s_tx_gap_cycles;

#if HAL_SPI_TX_IRQ_ENABLE
static volatile bool s_busy;
static hal_spi_tx_done_cb_t s_tx_done_cb;

SDK_DECLARE_EXT_ISR_M(BOARD_APP_SPI_IRQ, hal_spi_isr)

void hal_spi_isr(void)
{
    uint32_t status = spi_get_interrupt_status(HAL_SPI_BASE);

    if ((status & SPI_INTRST_ENDINT_MASK) == 0U) {
        return;
    }

    spi_clear_interrupt_status(HAL_SPI_BASE, spi_end_int);
    spi_disable_interrupt(HAL_SPI_BASE, spi_end_int);
    dma_disable_channel(HAL_SPI_DMA_CONTROLLER, HAL_SPI_DMA_CHANNEL);
    spi_disable_tx_dma(HAL_SPI_BASE);
    s_busy = false;

    if (s_tx_done_cb != NULL) {
        s_tx_done_cb();
    }
}
#endif

static hpm_stat_t hal_spi_abort_transfer(void)
{
    dma_abort_channel(HAL_SPI_DMA_CONTROLLER, (1UL << HAL_SPI_DMA_CHANNEL));
    dma_clear_transfer_status(HAL_SPI_DMA_CONTROLLER, HAL_SPI_DMA_CHANNEL);
    spi_reset(HAL_SPI_BASE);
    (void)spi_poll_reset_complete(HAL_SPI_BASE, spi_reset_all, HAL_SPI_TX_TIMEOUT_US / HAL_SPI_POLL_INTERVAL_US);
#if HAL_SPI_TX_IRQ_ENABLE
    spi_disable_interrupt(HAL_SPI_BASE, spi_end_int);
    spi_clear_interrupt_status(HAL_SPI_BASE, spi_end_int);
#endif
    return status_fail;
}

hpm_stat_t hal_spi_init(void)
{
    spi_timing_config_t timing_config = {0};
    spi_format_config_t format_config = {0};
    hpm_stat_t stat;
    uint32_t spi_clock_hz;
    uint32_t cpu_clock_hz;

    board_init_spi_pins(HAL_SPI_BASE);
    (void)board_init_spi_clock(HAL_SPI_BASE);

    /* 24MHz / 4 = 6MHz，固定时钟源，保证 WS2812 编码时序不随 PLL 变化 */
    stat = clock_set_source_divider(HAL_SPI_CLOCK_NAME, clk_src_osc24m, 4U);
    if (stat != status_success) {
        return stat;
    }

    spi_clock_hz = clock_get_frequency(HAL_SPI_CLOCK_NAME);
    if (spi_clock_hz != HAL_SPI_BITRATE_HZ) {
        return status_fail;
    }

    spi_master_get_default_timing_config(&timing_config);
    timing_config.master_config.cs2sclk = spi_cs2sclk_half_sclk_4;
    timing_config.master_config.clk_src_freq_in_hz = spi_clock_hz;
    timing_config.master_config.sclk_freq_in_hz = HAL_SPI_BITRATE_HZ;
    stat = spi_master_timing_init(HAL_SPI_BASE, &timing_config);
    if (stat != status_success) {
        return stat;
    }

    spi_master_get_default_format_config(&format_config);
    format_config.common_config.data_len_in_bits = HAL_SPI_DATA_BITS;
    format_config.common_config.data_merge = false;
    format_config.common_config.mosi_bidir = false;
    format_config.common_config.lsb = false;
    format_config.common_config.mode = spi_master_mode;
    format_config.common_config.cpol = spi_sclk_low_idle;
    format_config.common_config.cpha = spi_sclk_sampling_even_clk_edges;
    spi_format_init(HAL_SPI_BASE, &format_config);

    dma_clear_transfer_status(HAL_SPI_DMA_CONTROLLER, HAL_SPI_DMA_CHANNEL);
    dmamux_config(HAL_SPI_DMAMUX_CONTROLLER, HAL_SPI_DMA_MUX_CHANNEL, HAL_SPI_TX_DMA, true);

#if HAL_SPI_TX_IRQ_ENABLE
    intc_m_enable_irq_with_priority(BOARD_APP_SPI_IRQ, 1);
    s_busy = false;
#endif

    cpu_clock_hz = clock_get_frequency(clock_cpu0);
    if (cpu_clock_hz == 0U) {
        return status_fail;
    }
    s_tx_gap_cycles = (((uint64_t)cpu_clock_hz * HAL_SPI_TX_GAP_US) + 999999ULL) / 1000000ULL;
    s_tx_start_cycle = 0;

    s_inited = true;
    return status_success;
}

hpm_stat_t hal_spi_write(const uint8_t *data, uint32_t size)
{
    dma_handshake_config_t dma_config = {0};
    spi_control_config_t control_config = {0};
    hpm_stat_t stat;
    uint32_t dma_status;
    uint64_t now_cycle;

    if ((!s_inited) || (data == NULL) || (size == 0U)) {
        return status_invalid_argument;
    }

    /* 在发送开头检查与上一帧起始时刻的间隔，帧间低电平即 WS2812 复位时间 */
    now_cycle = hpm_csr_get_core_mcycle();
    if ((now_cycle - s_tx_start_cycle) < s_tx_gap_cycles) {
        return status_spi_master_busy;
    }

#if HAL_SPI_TX_IRQ_ENABLE
    if (s_busy) {
        return status_spi_master_busy;
    }
    s_busy = true;
#endif

    /* 上一帧还没发完或 SPI 仍忙时复位一次，避免数据错位 */
    dma_status = dma_check_transfer_status(HAL_SPI_DMA_CONTROLLER, HAL_SPI_DMA_CHANNEL);
    if (((dma_status & DMA_CHANNEL_STATUS_TC) == 0U) || spi_is_active(HAL_SPI_BASE)) {
        (void)hal_spi_abort_transfer();
    }

    spi_master_get_default_control_config(&control_config);
    control_config.master_config.cmd_enable = false;
    control_config.master_config.addr_enable = false;
    control_config.common_config.tx_dma_enable = true;
    control_config.common_config.rx_dma_enable = false;
    control_config.common_config.trans_mode = spi_trans_write_only;
    control_config.common_config.data_phase_fmt = spi_single_io_mode;
    control_config.common_config.dummy_cnt = spi_dummy_count_1;
    stat = spi_setup_dma_transfer(HAL_SPI_BASE, &control_config, NULL, NULL, size, 1U);
    if (stat != status_success) {
        (void)hal_spi_abort_transfer();
#if HAL_SPI_TX_IRQ_ENABLE
        s_busy = false;
#endif
        return stat;
    }

    dma_clear_transfer_status(HAL_SPI_DMA_CONTROLLER, HAL_SPI_DMA_CHANNEL);
    dma_default_handshake_config(HAL_SPI_DMA_CONTROLLER, &dma_config);
    dma_config.ch_index = HAL_SPI_DMA_CHANNEL;
    dma_config.dst = (uint32_t)&HAL_SPI_BASE->DATA;
    dma_config.dst_fixed = true;
    dma_config.src = core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)data);
    dma_config.src_fixed = false;
    dma_config.data_width = DMA_TRANSFER_WIDTH_BYTE;
    dma_config.size_in_byte = size;
    dma_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
    stat = dma_setup_handshake(HAL_SPI_DMA_CONTROLLER, &dma_config, true);
    if (stat != status_success) {
        (void)hal_spi_abort_transfer();
#if HAL_SPI_TX_IRQ_ENABLE
        s_busy = false;
#endif
        return stat;
    }

    s_tx_start_cycle = now_cycle;

#if HAL_SPI_TX_IRQ_ENABLE
    spi_enable_interrupt(HAL_SPI_BASE, spi_end_int);
    if ((spi_get_interrupt_status(HAL_SPI_BASE) & SPI_INTRST_ENDINT_MASK) != 0U) {
        hal_spi_isr();
    }
#endif
    return status_success;
}

#if HAL_SPI_TX_IRQ_ENABLE
void hal_spi_set_tx_done_callback(hal_spi_tx_done_cb_t cb)
{
    s_tx_done_cb = cb;
}
#endif
