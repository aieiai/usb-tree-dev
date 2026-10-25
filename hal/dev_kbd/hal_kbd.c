#include <string.h>

#include "hal_kbd.h"

#include "usb_osal.h"
#include "usbd_core.h"
#include "usbd_hid.h"

#include "hid_descriptor.h"


static struct usbd_interface s_intf;

/* IN 端点的发送完成信号量，以及发送缓冲区。
 * 缓冲区会被端点当作 DMA 源地址使用，必须常驻且地址固定，不能放在栈上。 */
static usb_osal_sem_t s_tx_sem;
static uint8_t s_tx_buf[HAL_KBD_REPORT_MAX] __attribute__((aligned(4)));

/* LED Output 报告：OUT 端点每收到一包都要重新装载，否则只能收到第一包 */
static uint8_t s_out_buf[HAL_KBD_LED_OUT_MAX] __attribute__((aligned(4)));
static volatile uint8_t s_leds;


static void ep_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;

    /* 中断上下文，tx_semaphore_put 可在 ISR 中调用 */
    if (s_tx_sem != NULL) {
        usb_osal_sem_give(s_tx_sem);
    }
}

static void ep_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;

    /* Output 报告格式：[Report ID][LED 位图] */
    if (nbytes >= 2U) {
        s_leds = s_out_buf[1];
    } else if (nbytes == 1U) {
        s_leds = s_out_buf[0];
    }

    if (usb_device_is_configured(busid)) {
        (void)usbd_ep_start_read(busid, INTF_KBD_EPO, s_out_buf, sizeof(s_out_buf));
    }
}

static void hal_kbd_init(uint8_t busid)
{
    s_tx_sem = usb_osal_sem_create(0);
    if (s_tx_sem == NULL) {
        USB_LOG_ERR("kbd tx semaphore create failed\r\n");
    }

    usbd_add_interface(busid, usbd_hid_init_intf(busid, &s_intf, g_desc_rpt_kbd.buf, g_desc_rpt_kbd.len));
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_KBD_EPI, .ep_cb = ep_in_callback });
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_KBD_EPO, .ep_cb = ep_out_callback });
}

static void hal_kbd_configured(uint8_t busid)
{
    (void)usbd_ep_start_read(busid, INTF_KBD_EPO, s_out_buf, sizeof(s_out_buf));
}

const struct hal_usb_function hal_kbd_function = {
    .intf_num = INTF_KBD_NUM,
    .name = "kbd",
    .init = hal_kbd_init,
    .configured = hal_kbd_configured,
    .event = NULL,
};

int hal_kbd_send(const uint8_t *report, uint16_t len, uint32_t timeout_ms)
{
    if ((report == NULL) || (len == 0U) || (len > sizeof(s_tx_buf))) {
        return -1;
    }
    if (!usb_device_is_configured(CONFIG_USBD_BUSID)) {
        return -2;
    }

    /* 丢弃上一次可能残留的完成信号，避免误判本次传输已完成 */
    while (usb_osal_sem_take(s_tx_sem, 0) == 0) {
    }

    memcpy(s_tx_buf, report, len);

    if (usbd_ep_start_write(CONFIG_USBD_BUSID, INTF_KBD_EPI, s_tx_buf, len) < 0) {
        return -3;
    }

    return (usb_osal_sem_take(s_tx_sem, timeout_ms) == 0) ? 0 : -4;
}

uint8_t hal_kbd_get_leds(void)
{
    return s_leds;
}
