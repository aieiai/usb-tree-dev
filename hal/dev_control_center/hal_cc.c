#include <string.h>

#include "hal_cc.h"

#include "usb_osal.h"
#include "usbd_core.h"
#include "usbd_hid.h"

#include "hid_descriptor.h"

#if !HAL_USB_CC_WINUSB_ENABLE
extern const usb_descriptor_t g_desc_rpt_cc;
#endif

static struct usbd_interface s_intf;

static uint8_t s_rx_buf[INTF_CC_EP_SIZE] __attribute__((aligned(4)));
static uint8_t s_ring[HAL_CC_RX_RING_SIZE];
static volatile uint32_t s_ring_head;
static volatile uint32_t s_ring_tail;
static volatile uint32_t s_dropped;


static void ring_push(const uint8_t *src, uint32_t len)
{
    size_t flag;
    uint32_t space;
    uint32_t pos;
    uint32_t first;

    flag = usb_osal_enter_critical_section();
    space = HAL_CC_RX_RING_SIZE - (uint32_t)(s_ring_tail - s_ring_head);
    if (space < len) {
        s_dropped += len;
        usb_osal_leave_critical_section(flag);
        return;
    }
    pos = s_ring_tail & (HAL_CC_RX_RING_SIZE - 1U);
    first = HAL_CC_RX_RING_SIZE - pos;
    if (first > len) {
        first = len;
    }
    memcpy(&s_ring[pos], src, first);
    if (len > first) {
        memcpy(&s_ring[0], src + first, len - first);
    }
    s_ring_tail += len;
    usb_osal_leave_critical_section(flag);
}

static void ep_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
}

static void ep_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;

    if ((nbytes > 0U) && (nbytes <= sizeof(s_rx_buf))) {
        ring_push(s_rx_buf, nbytes);
    }

    if (usb_device_is_configured(busid)) {
        (void)usbd_ep_start_read(busid, INTF_CC_EPO, s_rx_buf, sizeof(s_rx_buf));
    }
}

static void hal_cc_init(uint8_t busid)
{
#if HAL_USB_CC_WINUSB_ENABLE
    usbd_add_interface(busid, &s_intf);
#else
    usbd_add_interface(busid, usbd_hid_init_intf(busid, &s_intf, g_desc_rpt_cc.buf, g_desc_rpt_cc.len));
#endif
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_CC_EPI, .ep_cb = ep_in_callback });
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_CC_EPO, .ep_cb = ep_out_callback });
}

static void hal_cc_configured(uint8_t busid)
{
    (void)usbd_ep_start_read(busid, INTF_CC_EPO, s_rx_buf, sizeof(s_rx_buf));
}

const struct hal_usb_function hal_cc_function = {
    .intf_num = INTF_CC_NUM,
    .name = "cc",
    .init = hal_cc_init,
    .configured = hal_cc_configured,
    .event = NULL,
};

int hal_cc_read(uint8_t *buf, uint32_t len)
{
    size_t flag;
    uint32_t used;
    uint32_t n;
    uint32_t pos;
    uint32_t first;

    if ((buf == NULL) || (len == 0U)) {
        return -1;
    }

    flag = usb_osal_enter_critical_section();
    used = s_ring_tail - s_ring_head;
    n = (used < len) ? used : len;
    pos = s_ring_head & (HAL_CC_RX_RING_SIZE - 1U);
    first = HAL_CC_RX_RING_SIZE - pos;
    if (first > n) {
        first = n;
    }
    memcpy(buf, &s_ring[pos], first);
    if (n > first) {
        memcpy(buf + first, &s_ring[0], n - first);
    }
    s_ring_head += n;
    usb_osal_leave_critical_section(flag);

    return (int)n;
}

uint32_t hal_cc_dropped(void)
{
    return s_dropped;
}
