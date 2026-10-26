#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hal_dt.h"

#include "usb_config.h"
#include "usbd_core.h"
#include "usbd_hid.h"
#include "usb_osal.h"

#include "hid_descriptor.h"

#if !HAL_USB_DT_WINUSB_ENABLE
extern const usb_descriptor_t g_desc_rpt_dt;
#endif

#if ((HAL_DT_RING_SIZE & (HAL_DT_RING_SIZE - 1U)) != 0U) || ((HAL_DT_RX_RING_SIZE & (HAL_DT_RX_RING_SIZE - 1U)) != 0U)
#error "HAL_DT_RING_SIZE / HAL_DT_RX_RING_SIZE must be power of two"
#endif

#define HAL_DT_PRINTF_MAX      (128U)

/* 简易环形缓冲：head 只由消费者写，tail 只由生产者写，并发区间用临界区保护 */
struct dt_ring {
    uint8_t *buf;
    uint32_t size;
    volatile uint32_t head;
    volatile uint32_t tail;
};

static uint8_t s_tx_ring_buf[HAL_DT_RING_SIZE];
static uint8_t s_rx_ring_buf[HAL_DT_RX_RING_SIZE];

static struct dt_ring s_tx_ring = {
    .buf = s_tx_ring_buf,
    .size = HAL_DT_RING_SIZE,
};
static struct dt_ring s_rx_ring = {
    .buf = s_rx_ring_buf,
    .size = HAL_DT_RX_RING_SIZE,
};

/* 端点缓冲区会被当作 DMA 源/目的地址使用，必须常驻且地址对齐 */
static uint8_t s_tx_buf[HAL_DT_PACKET_SIZE] __attribute__((aligned(4)));
static uint8_t s_rx_buf[HAL_DT_RX_PACKET_SIZE] __attribute__((aligned(4)));

static struct usbd_interface s_dt_intf;

static usb_osal_sem_t s_tx_pending_sem; /* 环形缓冲有待发送数据 / 主机已配置 */
static usb_osal_sem_t s_tx_done_sem;    /* 批量 IN 传输完成 */
static usb_osal_sem_t s_rx_ready_sem;   /* OUT 环形缓冲有新数据 */

static volatile uint32_t s_dropped;
static uint8_t s_busid;


static uint32_t ring_push(struct dt_ring *ring, const uint8_t *src, uint32_t len)
{
    uint32_t space;
    uint32_t pos;
    uint32_t first;
    size_t flag;

    flag = usb_osal_enter_critical_section();
    space = ring->size - (uint32_t)(ring->tail - ring->head);
    if (space < len) {
        usb_osal_leave_critical_section(flag);
        return 0;
    }

    pos = ring->tail & (ring->size - 1U);
    first = ring->size - pos;
    if (first > len) {
        first = len;
    }
    memcpy(&ring->buf[pos], src, first);
    if (len > first) {
        memcpy(&ring->buf[0], src + first, len - first);
    }
    ring->tail += len;
    usb_osal_leave_critical_section(flag);

    return len;
}

static uint32_t ring_pop(struct dt_ring *ring, uint8_t *dst, uint32_t len)
{
    uint32_t used;
    uint32_t n;
    uint32_t pos;
    uint32_t first;
    size_t flag;

    flag = usb_osal_enter_critical_section();
    used = ring->tail - ring->head;
    n = (used < len) ? used : len;

    pos = ring->head & (ring->size - 1U);
    first = ring->size - pos;
    if (first > n) {
        first = n;
    }
    memcpy(dst, &ring->buf[pos], first);
    if (n > first) {
        memcpy(dst + first, &ring->buf[0], n - first);
    }
    ring->head += n;
    usb_osal_leave_critical_section(flag);

    return n;
}

static void ep_dt_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;

    /* 中断上下文，tx_semaphore_put 可在 ISR 中调用 */
    if (s_tx_done_sem != NULL) {
        usb_osal_sem_give(s_tx_done_sem);
    }
}

static void ep_dt_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;

    if ((nbytes > 0U) && (nbytes <= sizeof(s_rx_buf))) {
        if ((ring_push(&s_rx_ring, s_rx_buf, nbytes) == nbytes) && (s_rx_ready_sem != NULL)) {
            usb_osal_sem_give(s_rx_ready_sem);
        }
    }

    /* OUT 端点每收到一包就要重新装载，否则只能收到第一包 */
    if (usb_device_is_configured(busid)) {
        (void)usbd_ep_start_read(busid, INTF_DT_EPO, s_rx_buf, sizeof(s_rx_buf));
    }
}

/* 发送线程：把环形缓冲里的诊断数据按 HAL_DT_PACKET_SIZE 一包搬到批量 IN 端点。
 * 业务侧只往环形缓冲写，不会因为主机不读数据而被卡住。 */
static void hal_dt_thread(void *argument)
{
    (void)argument;

    while (1) {
        (void)usb_osal_sem_take(s_tx_pending_sem, HAL_DT_TX_TIMEOUT_MS);

        if (!usb_device_is_configured(s_busid)) {
            continue;
        }

        for (;;) {
            uint32_t len = ring_pop(&s_tx_ring, s_tx_buf, sizeof(s_tx_buf));

            if (len == 0U) {
                break;
            }

            /* 清掉可能残留的完成信号，避免误判本次传输已完成 */
            (void)usb_osal_sem_take(s_tx_done_sem, 0);

            if (usbd_ep_start_write(s_busid, INTF_DT_EPI, s_tx_buf, len) < 0) {
                s_dropped += len;
                break;
            }

            if (usb_osal_sem_take(s_tx_done_sem, HAL_DT_TX_TIMEOUT_MS) != 0) {
                /* 主机长时间不取数据，丢弃这一包，避免整个通道卡死 */
                s_dropped += len;
                break;
            }
        }
    }
}

void hal_dt_usb_init(uint8_t busid)
{
    s_busid = busid;

    s_tx_pending_sem = usb_osal_sem_create(0);
    s_tx_done_sem = usb_osal_sem_create(0);
    s_rx_ready_sem = usb_osal_sem_create(0);
    if ((s_tx_pending_sem == NULL) || (s_tx_done_sem == NULL) || (s_rx_ready_sem == NULL)) {
        USB_LOG_ERR("dt semaphore create failed\r\n");
        return;
    }

    memset(&s_dt_intf, 0, sizeof(s_dt_intf));
#if HAL_USB_DT_WINUSB_ENABLE
    usbd_add_interface(busid, &s_dt_intf);
#else
    usbd_add_interface(busid, usbd_hid_init_intf(busid, &s_dt_intf, g_desc_rpt_dt.buf, g_desc_rpt_dt.len));
#endif
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_DT_EPI, .ep_cb = ep_dt_in_callback });
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_DT_EPO, .ep_cb = ep_dt_out_callback });

    if (usb_osal_thread_create("usb_dt", HAL_DT_THREAD_STACK, HAL_DT_THREAD_PRIO, hal_dt_thread, NULL) == NULL) {
        USB_LOG_ERR("dt thread create failed\r\n");
    }
}

void hal_dt_usb_configured(void)
{
    if (s_tx_pending_sem == NULL) {
        return;
    }

    /* 主机每次重新配置后都要重新装载 OUT 端点 */
    (void)usbd_ep_start_read(s_busid, INTF_DT_EPO, s_rx_buf, sizeof(s_rx_buf));
    usb_osal_sem_give(s_tx_pending_sem);
}

void hal_dt_write(const void *data, uint32_t len)
{
    if ((data == NULL) || (len == 0U) || (s_tx_pending_sem == NULL)) {
        return;
    }

    if (ring_push(&s_tx_ring, (const uint8_t *)data, len) != len) {
        /* 缓冲满：整包丢弃，保证上位机看到的诊断记录是完整的 */
        s_dropped += len;
        return;
    }

    usb_osal_sem_give(s_tx_pending_sem);
}

void hal_dt_puts(const char *str)
{
    if (str != NULL) {
        hal_dt_write(str, (uint32_t)strlen(str));
    }
}

void hal_dt_printf(const char *fmt, ...)
{
    char buf[HAL_DT_PRINTF_MAX];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0) {
        return;
    }
    if ((uint32_t)len >= sizeof(buf)) {
        len = (int)sizeof(buf) - 1; /* 超长记录截断，不破坏缓冲里已有的内容 */
    }
    hal_dt_write(buf, (uint32_t)len);
}

uint32_t hal_dt_dropped(void)
{
    return s_dropped;
}

int hal_dt_read(void *buf, uint32_t len, uint32_t timeout_ms)
{
    uint32_t n;

    if ((buf == NULL) || (len == 0U) || (s_rx_ready_sem == NULL)) {
        return -1;
    }
    if (usb_osal_sem_take(s_rx_ready_sem, timeout_ms) != 0) {
        return -1;
    }

    n = ring_pop(&s_rx_ring, (uint8_t *)buf, len);
    return (int)n;
}

/* ---- 注册给 hal_usb 的功能回调 ---------------------------------------- */
static void hal_dt_init(uint8_t busid)
{
    hal_dt_usb_init(busid);
}

static void hal_dt_configured(uint8_t busid)
{
    (void)busid;
    hal_dt_usb_configured();
}

const struct hal_usb_function hal_dt_function = {
    .intf_num = INTF_DT_NUM,
    .name = "dt",
    .init = hal_dt_init,
    .configured = hal_dt_configured,
    .event = NULL,
};
