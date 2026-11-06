#include "hal_cc.h"

#include "../../hal_usb.h"
#include "../descriptor/hid_descriptor.h"

#include "hpm_interrupt.h"
#include "usbd_core.h"
#include "usbd_hid.h"

#if !HAL_USB_CC_WINUSB_ENABLE
extern const usb_descriptor_t g_desc_rpt_cc;
#endif


static struct usbd_interface s_intf;
static uint8_t s_rx_buf[INTF_CC_EP_SIZE] __attribute__((aligned(4)));
static uint8_t s_tx_buf[INTF_CC_EP_SIZE] __attribute__((aligned(4)));
static hal_cc_callbacks_t ctx_cb;
static volatile bool s_tx_busy;
static uint8_t s_busid;
static bool s_initialized;

static void hal_cc_init(uint8_t busid);
static void hal_cc_configured(uint8_t busid);

const hal_usb_fn_t hal_cc_function = {
    .intf_num = INTF_CC_NUM,
    .name = "cc",
    .init = hal_cc_init,
    .configured = hal_cc_configured,
    .event = NULL,
};


static void ep_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;

    s_tx_busy = false;
    if (ctx_cb.tx_complete != NULL) {
        ctx_cb.tx_complete(rot_state_success, (uint16_t)nbytes, ctx_cb.ctx);
    }
}

static void ep_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;

    if ((nbytes > 0U) && (nbytes <= sizeof(s_rx_buf)) && (ctx_cb.rx != NULL)) {
        ctx_cb.rx(s_rx_buf, (uint16_t)nbytes, ctx_cb.ctx);
    }

    if (usb_device_is_configured(busid)) {
        (void)usbd_ep_start_read(busid, INTF_CC_EPO, s_rx_buf, sizeof(s_rx_buf));
    }
}

static void hal_cc_init(uint8_t busid)
{
    s_busid = busid;
    s_initialized = true;

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
    s_tx_busy = false;
    (void)usbd_ep_start_read(busid, INTF_CC_EPO, s_rx_buf, sizeof(s_rx_buf));
    if (ctx_cb.configured != NULL) {
        ctx_cb.configured(ctx_cb.ctx);
    }
}

int hal_cc_register_callbacks(const hal_cc_callbacks_t *cb)
{
    uint32_t irq_state = disable_global_irq(CSR_MSTATUS_MIE_MASK);

    if (cb == NULL) {
        memset(&ctx_cb, 0, sizeof(ctx_cb));
    }
    else {
        ctx_cb = *cb;
    }
    restore_global_irq(irq_state);
    return rot_state_success;
}

int hal_cc_send(const void *data, uint32_t len)
{
    uint32_t irq_state;

    if ((data == NULL) || (len == 0U) || (len > INTF_CC_EP_SIZE)) {
        return rot_state_hal_cc_arg;
    }
    if (!s_initialized || !usb_device_is_configured(s_busid)) {
        return rot_state_hal_cc_not_ready;
    }

    irq_state = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    if (s_tx_busy) {
        restore_global_irq(irq_state);
        return rot_state_hal_cc_busy;
    }
    s_tx_busy = true;
    restore_global_irq(irq_state);

    memcpy(s_tx_buf, data, len);
    if (usbd_ep_start_write(s_busid, INTF_CC_EPI, s_tx_buf, len) < 0) {
        s_tx_busy = false;
        return rot_state_hal_cc_start;
    }
    return rot_state_success;
}

bool hal_cc_is_busy(void)
{
    return s_tx_busy;
}
