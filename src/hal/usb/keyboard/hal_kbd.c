#include "hal_kbd.h"
#include "../../hal_usb.h"
#include "../descriptor/hid_descriptor.h"

#include "usbd_core.h"
#include "usbd_hid.h"

#include "tx_api.h"


static struct usbd_interface s_intf;

/* IN 端点的发送完成信号量，以及发送缓冲区。
 * 缓冲区会被端点当作 DMA 源地址使用，必须常驻且地址固定，不能放在栈上。 */
static TX_SEMAPHORE s_tx_sem;
static uint8_t s_tx_buf[HAL_KBD_REPORT_MAX] __attribute__((aligned(4)));

static uint8_t s_out_buf[HAL_KBD_LED_OUT_MAX] __attribute__((aligned(4)));
static volatile uint8_t s_leds;
static volatile uint32_t s_usb_generation;


static void ep_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;

    if (tx_semaphore_put(&s_tx_sem) != TX_SUCCESS) {
        USB_LOG_ERR("tx_semaphore_put failed\r\n");
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
    if (tx_semaphore_create(&s_tx_sem, "kbdtx_sem", 0U) != TX_SUCCESS) {
        USB_LOG_ERR("tx_semaphore_create failed\r\n");
    }

    usbd_add_interface(busid, usbd_hid_init_intf(busid, &s_intf, g_desc_rpt_kbd.buf, g_desc_rpt_kbd.len));
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_KBD_EPI, .ep_cb = ep_in_callback });
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_KBD_EPO, .ep_cb = ep_out_callback });
}

static void hal_kbd_configured(uint8_t busid)
{
    (void)usbd_ep_start_read(busid, INTF_KBD_EPO, s_out_buf, sizeof(s_out_buf));
}

static void hal_kbd_event(uint8_t busid, uint8_t event)
{
    (void)busid;

    if ((event == USBD_EVENT_RESET) || (event == USBD_EVENT_DISCONNECTED) || (event == USBD_EVENT_CONFIGURED)) {
        s_usb_generation++;
    }
}

const hal_usb_fn_t hal_kbd_function = {
    .intf_num = INTF_KBD_NUM,
    .name = "kbd",
    .init = hal_kbd_init,
    .configured = hal_kbd_configured,
    .event = hal_kbd_event,
};

int hal_kbd_send(const uint8_t *report, uint16_t len, uint32_t timeout_ms)
{
    if ((report == NULL) || (len == 0U) || (len > sizeof(s_tx_buf))) {
        return -1;
    }
    if (!usb_device_is_configured(CONFIG_USBD_BUSID)) {
        return -2;
    }

    /* 丢弃上一次可能残留的完成信号，避免误判本次传输已完成。
     * 这里必须非阻塞取，否则每次发送都会先空等一个超时周期。 */
    while (tx_semaphore_get(&s_tx_sem, TX_NO_WAIT) == TX_SUCCESS) {
    }

    memcpy(s_tx_buf, report, len);

    if (usbd_ep_start_write(CONFIG_USBD_BUSID, INTF_KBD_EPI, s_tx_buf, len) < 0) {
        return -3;
    }

    return (tx_semaphore_get(&s_tx_sem, timeout_ms) == TX_SUCCESS) ? 0 : -4;
}

uint8_t hal_kbd_get_leds(void)
{
    return s_leds;
}

uint32_t hal_kbd_usb_generation(void)
{
    return s_usb_generation;
}
