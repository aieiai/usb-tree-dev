#include "hal_uart.h"
#include "board.h"

#include "hpm_clock_drv.h"
#include "hpm_uart_drv.h"
#include "hpm_dmav2_drv.h"
#include "hpm_dmamux_drv.h"
#include "hpm_l1c_drv.h"
#include "hpm_common.h"

/* test uart */
#define BOARD_TEST_UART                   HPM_UART2
#define BOARD_TEST_UART_CLK_NAME          clock_uart2
#define BOARD_TEST_UART_TX_DMA_REQ        HPM_DMA_SRC_UART2_TX
#define BOARD_TEST_UART_RX_DMA_REQ        HPM_DMA_SRC_UART2_RX
#define BOARD_TEST_UART_BAUDRATE          (115200UL)

#define BOARD_TEST_UART_DMA_CONTROLLER    HPM_HDMA
#define BOARD_TEST_UART_DMAMUX_CONTROLLER HPM_DMAMUX
#define BOARD_TEST_UART_TX_DMA_CHN        (5U)
#define BOARD_TEST_UART_RX_DMA_CHN        (4U)
#define BOARD_TEST_UART_TX_DMAMUX_CHN     DMA_SOC_CHN_TO_DMAMUX_CHN(BOARD_TEST_UART_DMA_CONTROLLER, BOARD_TEST_UART_TX_DMA_CHN)
#define BOARD_TEST_UART_RX_DMAMUX_CHN     DMA_SOC_CHN_TO_DMAMUX_CHN(BOARD_TEST_UART_DMA_CONTROLLER, BOARD_TEST_UART_RX_DMA_CHN)
#define BOARD_TEST_UART_DMA_IRQ           IRQn_HDMA


static UART_Type* uart_base = NULL;


//#define TEST_BUFFER_SIZE   (256U)
//ATTR_PLACE_AT_NONCACHEABLE uint8_t uart_buff[TEST_BUFFER_SIZE];

// volatile bool uart_tx_dma_done;
// volatile bool uart_rx_dma_done;


// void uart_dma_isr(void)
// {
    // hpm_stat_t rx_stat = dma_check_transfer_status(BOARD_TEST_UART_DMA_CONTROLLER, BOARD_TEST_UART_RX_DMA_CHN);
    // if (rx_stat & DMA_CHANNEL_STATUS_TC) {
    //     uart_rx_dma_done = true;
    //     // dma_clear_transfer_status(BOARD_TEST_UART_DMA_CONTROLLER, BOARD_TEST_UART_RX_DMA_CHN);
    // }
    // hpm_stat_t tx_stat = dma_check_transfer_status(BOARD_TEST_UART_DMA_CONTROLLER, BOARD_TEST_UART_TX_DMA_CHN);
    // if (tx_stat & DMA_CHANNEL_STATUS_TC) {
    //     uart_tx_dma_done = true;
    //     // dma_clear_transfer_status(BOARD_TEST_UART_DMA_CONTROLLER, BOARD_TEST_UART_TX_DMA_CHN);
    // }
// }
//SDK_DECLARE_EXT_ISR_M(TEST_UART_DMA_IRQ, dma_isr)


hpm_stat_t uart_tx_trigger_dma(uint8_t *buff, uint32_t size)
{
    if (uart_base == NULL)
    {
        return status_success;
    }
    hpm_stat_t sta = status_fail;
    // if (uart_tx_dma_done)
    {
        dma_handshake_config_t config;

        dma_default_handshake_config(BOARD_TEST_UART_DMA_CONTROLLER, &config);
        config.ch_index = BOARD_TEST_UART_TX_DMA_CHN;
        config.dst = (uint32_t)&uart_base->THR;
        config.dst_fixed = true;
        config.src = core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)buff);
        config.src_fixed = false;
        config.data_width = DMA_TRANSFER_WIDTH_BYTE;
        config.size_in_byte = size;
        config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        // uart_tx_dma_done = false;
        sta = dma_setup_handshake(BOARD_TEST_UART_DMA_CONTROLLER, &config, true);
        // if (sta != status_success)
        // {
        //     uart_tx_dma_done = true;
        // }
    }
    return sta;
}

hpm_stat_t uart_rx_trigger_dma(uint8_t *buff, uint32_t size)
{
    if (uart_base == NULL)
    {
        return status_success;
    }
    dma_handshake_config_t config;

    dma_default_handshake_config(BOARD_TEST_UART_DMA_CONTROLLER, &config);
    config.ch_index = BOARD_TEST_UART_RX_DMA_CHN;
    config.dst = core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)buff);
    config.dst_fixed = false;
    config.src = (uint32_t)&uart_base->RBR;
    config.src_fixed = true;
    config.data_width = DMA_TRANSFER_WIDTH_BYTE;
    config.size_in_byte = size;
    config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
    // uart_rx_dma_done = false;

    return dma_setup_handshake(BOARD_TEST_UART_DMA_CONTROLLER, &config, true);
}


hpm_stat_t hal_uart_init(void)
{
    hpm_stat_t stat = status_fail;
    uart_config_t config = {0};

    board_init_uart(BOARD_TEST_UART);
    uart_default_config(BOARD_TEST_UART, &config);
    config.fifo_enable = true;
    config.dma_enable = true;
    config.rx_enable = true;
    config.src_freq_in_hz = clock_get_frequency(BOARD_TEST_UART_CLK_NAME);
    config.baudrate = BOARD_TEST_UART_BAUDRATE;
    config.tx_fifo_level = uart_tx_fifo_trg_not_full;
    config.rx_fifo_level = uart_rx_fifo_trg_not_empty;
    stat = uart_init(BOARD_TEST_UART, &config);
    if (stat == status_success) {
        uart_base = BOARD_TEST_UART;
        dmamux_config(BOARD_TEST_UART_DMAMUX_CONTROLLER, BOARD_TEST_UART_RX_DMAMUX_CHN, BOARD_TEST_UART_RX_DMA_REQ, true);
        dmamux_config(BOARD_TEST_UART_DMAMUX_CONTROLLER, BOARD_TEST_UART_TX_DMAMUX_CHN, BOARD_TEST_UART_TX_DMA_REQ, true);
    }


    uint8_t tx_buff[14] = "hello world\r\n";
    uart_tx_trigger_dma(tx_buff, 14);

    
    return stat;
}




