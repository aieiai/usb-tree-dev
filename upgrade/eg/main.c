/*
 * Copyright (c) 2022 hpmicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "uart_xmodem.h"
#include "flash_map.h"
#include "ota_api.h"
#include "hal_uart.h"

 /* Write file to flash */
static int writefile(unsigned char* data, int len)
{
    /* First packet contain file header info */
    ota_board_auto_write((void*)data, len);
    return 0;
}

int main(void)
{
    int ret = 0;
    uint8_t ota_index;
    board_init();
    hal_uart_init();

    printf("hello world!\n");
    ota_board_flash_init();
    ota_index = ota_check_current_otaindex();
    printf("now runing OTA%d...\n", ota_index+1);
    uint8_t buf[100];
    uint16_t len;


    ret = xmodemReceive(writefile);
    if (ret > 0)
    {
       len = sprintf((char *)buf, "reset\n");
       uart_tx_trigger_dma(buf, len);

       ota_board_complete_reset();
    }
    else
    {
       len = sprintf((char *)buf, "xmodem err ret: %d!\n", ret);
        uart_tx_trigger_dma(buf, len);
    }
    while (1)
    {

    }
    return 0;
}


