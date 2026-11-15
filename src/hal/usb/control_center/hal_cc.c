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

typedef struct {
    hal_cc_rx_cb_t rx;
    hal_cc_tx_complete_cb_t tx_complete;
    hal_cc_config_cb_t config;
    void *ctx;
} hal_cc_callbacks_t;

static hal_cc_callbacks_t use_cb = { NULL };

static volatile bool s_tx_busy;
static uint8_t s_busid;
static bool s_initialized;

_Static_assert((INTF_CC_EP_SIZE >= 64U) && (INTF_CC_EP_SIZE <= 1024U) &&
               ((INTF_CC_EP_SIZE & (INTF_CC_EP_SIZE - 1U)) == 0U),
               "CC endpoint size must be a power of two from 64 to 1024 bytes");

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
    if (use_cb.tx_complete != NULL) {
        use_cb.tx_complete(rotsta_success, (uint16_t)nbytes, use_cb.ctx);
    }
}

static void ep_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;

    if ((nbytes > 0U) && (nbytes <= sizeof(s_rx_buf)) && (use_cb.rx != NULL)) {
        use_cb.rx(s_rx_buf, (uint16_t)nbytes, use_cb.ctx);
    }

    if (usb_device_is_configured(busid)) {
        (void)usbd_ep_start_read(busid, INTF_CC_EPO, s_rx_buf, sizeof(s_rx_buf));
    }
}

static void hal_cc_configured(uint8_t busid)
{
    s_tx_busy = false;
    (void)usbd_ep_start_read(busid, INTF_CC_EPO, s_rx_buf, sizeof(s_rx_buf));
    if (use_cb.config != NULL) {
        use_cb.config(use_cb.ctx);
    }
}

bool hal_cc_is_busy(void)
{
    return s_tx_busy;
}

int hal_cc_send(const void *data, uint32_t len)
{
    uint32_t irq_state;

    if ((data == NULL) || (len == 0U) || (len > INTF_CC_EP_SIZE)) {
        return rotsta_hal_cc_arg;
    }
    if (!s_initialized || !usb_device_is_configured(s_busid)) {
        return rotsta_hal_cc_not_ready;
    }

    irq_state = disable_global_irq(CSR_MSTATUS_MIE_MASK);
    if (s_tx_busy) {
        restore_global_irq(irq_state);
        return rotsta_hal_cc_busy;
    }
    s_tx_busy = true;
    restore_global_irq(irq_state);

    if (usbd_ep_start_write(s_busid, INTF_CC_EPI, data, len) < 0) {
        s_tx_busy = false;
        return rotsta_hal_cc_start;
    }
    return rotsta_success;
}

int hal_cc_register_callbacks(const hal_cc_rx_cb_t rx_cb, 
    const hal_cc_tx_complete_cb_t tx_complete_cb, 
    const hal_cc_config_cb_t configured_cb, 
    void *ctx)
{
    uint32_t irq_state = disable_global_irq(CSR_MSTATUS_MIE_MASK);

    use_cb.ctx = ctx;
    use_cb.rx = rx_cb;
    use_cb.tx_complete = tx_complete_cb;
    use_cb.config = configured_cb;

    restore_global_irq(irq_state);
    return rotsta_success;
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
