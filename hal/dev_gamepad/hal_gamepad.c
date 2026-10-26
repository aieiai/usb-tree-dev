#include <string.h>

#include "hal_gamepad.h"

#include "usb_osal.h"
#include "usbd_core.h"
#include "usbd_hid.h"

#include "hid_descriptor.h"

extern const usb_descriptor_t g_desc_rpt_gamepad;

static struct usbd_interface s_intf;

/* IN 端点的发送完成信号量，以及发送缓冲区。
 * 缓冲区会被端点当作 DMA 源地址使用，必须常驻且地址固定，不能放在栈上。 */
static usb_osal_sem_t s_tx_sem;
static uint8_t s_tx_buf[INTF_GAMEPAD_XBOX_RPT_LEN] __attribute__((aligned(4)));

/* Xbox 模式 OUT 端点：主机每下发一包震动数据都要重新装载 */
static uint8_t s_out_buf[INTF_GAMEPAD_XBOX_EPO_SIZE] __attribute__((aligned(4)));

/* 枚举完成时补发的首帧空闲报告专用缓冲，避免和随后的 hal_gamepad_send 抢 s_tx_buf */
static uint8_t s_boot_buf[INTF_GAMEPAD_XBOX_RPT_LEN] __attribute__((aligned(4)));

static volatile uint8_t s_rumble_left;
static volatile uint8_t s_rumble_right;

/* 最近一帧 OUT 报告：中断里只做缓存，解析 / 打印放到任务上下文，
 * 避免在 ISR 里跑 vsnprintf。 */
static uint8_t s_out_last[INTF_GAMEPAD_XBOX_EPO_SIZE];
static volatile uint32_t s_out_last_len;
static volatile uint32_t s_out_seq;
static uint32_t s_out_read_seq;


static uint16_t gamepad_encode_xbox(const hal_gamepad_state_t *st, uint8_t *p);


/* 摇杆轴统一裁剪到 HID 描述符声明的逻辑范围 -32767..32767 */
static int16_t clamp_axis(int32_t value)
{
    if (value > 32767) {
        return 32767;
    }
    if (value < -32767) {
        return -32767;
    }
    return (int16_t)value;
}

static void put_i16(uint8_t *p, int16_t v)
{
    p[0] = (uint8_t)((uint16_t)v & 0xFFU);
    p[1] = (uint8_t)((uint16_t)v >> 8);
}

static void ep_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;

    if (s_tx_sem != NULL) {
        usb_osal_sem_give(s_tx_sem);
    }
}

static void ep_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;

    /* 先缓存原始帧，交给任务上下文解析 */
    if ((nbytes > 0U) && (nbytes <= sizeof(s_out_last))) {
        size_t flag = usb_osal_enter_critical_section();

        memcpy(s_out_last, s_out_buf, nbytes);
        s_out_last_len = nbytes;
        s_out_seq++;

        usb_osal_leave_critical_section(flag);
    }

    /* Xbox 震动包：[0x00][0x08][0x00][左马达][右马达][0][0][0] */
    if ((nbytes >= 5U) && (s_out_buf[0] == 0x00U) && (s_out_buf[1] == 0x08U)) {
        s_rumble_left = s_out_buf[3];
        s_rumble_right = s_out_buf[4];
        hal_gamepad_rumble(s_rumble_left, s_rumble_right);
    }

    if (usb_device_is_configured(busid)) {
        (void)usbd_ep_start_read(busid, INTF_GAMEPAD_XBOX_EPO, s_out_buf, sizeof(s_out_buf));
    }
}

static void hal_gamepad_init(uint8_t busid)
{
    /* 模式切换会重新走一遍初始化，信号量只在第一次创建 */
    if (s_tx_sem == NULL) {
        s_tx_sem = usb_osal_sem_create(0);
        if (s_tx_sem == NULL) {
            USB_LOG_ERR("gamepad tx semaphore create failed\r\n");
        }
    }

    if (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) {
        /* vendor class 接口没有 HID class 回调；先清零，避免从 HID 模式切过来时残留 hid handler */
        memset(&s_intf, 0, sizeof(s_intf));
        usbd_add_interface(busid, &s_intf);
        usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_GAMEPAD_XBOX_EPI, .ep_cb = ep_in_callback });
        usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_GAMEPAD_XBOX_EPO, .ep_cb = ep_out_callback });
    } else {
        usbd_add_interface(busid, usbd_hid_init_intf(busid, &s_intf, g_desc_rpt_gamepad.buf, g_desc_rpt_gamepad.len));
        usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_GAMEPAD_HID_EPI, .ep_cb = ep_in_callback });
    }
}

static void hal_gamepad_configured(uint8_t busid)
{
    s_rumble_left = 0;
    s_rumble_right = 0;

    if (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) {
        const hal_gamepad_state_t idle = { .hat = HAL_GAMEPAD_HAT_NONE };
        int ret;

        (void)usbd_ep_start_read(busid, INTF_GAMEPAD_XBOX_EPO, s_out_buf, sizeof(s_out_buf));

        /* xusb22.sys 要收到首帧 IN 报告才会把设备注册到 Gaming Input（joy.cpl）；
         * 应用若还没开始发数据，这里先补一帧空闲状态。 */
        (void)gamepad_encode_xbox(&idle, s_boot_buf);
        ret = usbd_ep_start_write(busid, INTF_GAMEPAD_XBOX_EPI, s_boot_buf, INTF_GAMEPAD_XBOX_RPT_LEN);
        USB_LOG_RAW("[usb] gamepad xbox boot report tx=%d (ep=0x%02x, %u bytes)\r\n",
                    ret, INTF_GAMEPAD_XBOX_EPI, (unsigned)INTF_GAMEPAD_XBOX_RPT_LEN);
    }
}

/* 手柄未启用时不注册接口，配置描述符里也没有该接口 */
static uint8_t hal_gamepad_present(void)
{
    return (g_gamepad_mode != HAL_GAMEPAD_MODE_OFF) ? 1U : 0U;
}

const struct hal_usb_function hal_gamepad_function = {
    .intf_num = INTF_GAMEPAD_NUM,
    .name = "gamepad",
    .init = hal_gamepad_init,
    .configured = hal_gamepad_configured,
    .event = NULL,
    .present = hal_gamepad_present,
};

/* HID 模式 13 字节报告，布局与 REPORT_DESC_GAMEPAD 严格对应：
 * [0..1] buttons，[2] hat(低 4 位)，[3..10] lx/ly/rx/ry(int16)，
 * [11..12] lt/rt。 */
static uint16_t gamepad_encode_hid(const hal_gamepad_state_t *st, uint8_t *p)
{
    uint8_t hat = (st->hat <= 7U) ? st->hat : (uint8_t)HAL_GAMEPAD_HAT_NONE;

    p[0] = (uint8_t)(st->buttons & 0xFFU);
    p[1] = (uint8_t)(st->buttons >> 8);
    p[2] = (uint8_t)(hat & 0x0FU); /* 高 4 位是描述符里的常量 padding，保持 0 */

    put_i16(&p[3], clamp_axis(st->lx));
    put_i16(&p[5], (int16_t)-clamp_axis(st->ly)); /* HID 的 Y 轴正向朝下，上层约定朝上为正 */
    put_i16(&p[7], clamp_axis(st->rx));
    put_i16(&p[9], (int16_t)-clamp_axis(st->ry));
    p[11] = st->lt;
    p[12] = st->rt;

    return INTF_GAMEPAD_HID_RPT_LEN;
}

/* Xbox 360 20 字节输入报告：
 * [0]=0x00 报文类型，[1]=0x14 长度，[2..3] 按键/方向帽，[4..5] 扳机，
 * [6..13] 四个摇杆轴（int16 LE），[14..19] 保留。 */
static uint16_t gamepad_encode_xbox(const hal_gamepad_state_t *st, uint8_t *p)
{
    static const uint8_t hat_bits[8] = {
        0x01,       /* 上 */
        0x01 | 0x08,/* 右上 */
        0x08,       /* 右 */
        0x08 | 0x02,/* 右下 */
        0x02,       /* 下 */
        0x02 | 0x04,/* 左下 */
        0x04,       /* 左 */
        0x04 | 0x01  /* 左上 */
    };

    memset(p, 0, INTF_GAMEPAD_XBOX_RPT_LEN);

    p[1] = 0x14;

    if (st->hat <= 7U) {
        p[2] = hat_bits[st->hat];
    }
    if (st->buttons & HAL_GAMEPAD_BTN_START) {
        p[2] |= 0x10;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_BACK) {
        p[2] |= 0x20;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_L3) {
        p[2] |= 0x40;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_R3) {
        p[2] |= 0x80;
    }

    if (st->buttons & HAL_GAMEPAD_BTN_LB) {
        p[3] |= 0x01;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_RB) {
        p[3] |= 0x02;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_GUIDE) {
        p[3] |= 0x04;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_A) {
        p[3] |= 0x10;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_B) {
        p[3] |= 0x20;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_X) {
        p[3] |= 0x40;
    }
    if (st->buttons & HAL_GAMEPAD_BTN_Y) {
        p[3] |= 0x80;
    }

    p[4] = st->lt;
    p[5] = st->rt;

    /* Xbox 的 X/Y 轴向与上层约定一致：X 右为正，Y 上为正 */
    put_i16(&p[6], clamp_axis(st->lx));
    put_i16(&p[8], clamp_axis(st->ly));
    put_i16(&p[10], clamp_axis(st->rx));
    put_i16(&p[12], clamp_axis(st->ry));

    return INTF_GAMEPAD_XBOX_RPT_LEN;
}

uint16_t hal_gamepad_encode(const hal_gamepad_state_t *state, uint8_t *buf, uint16_t buf_len)
{
    uint16_t len;

    if ((state == NULL) || (buf == NULL)) {
        return 0;
    }
    if (g_gamepad_mode == HAL_GAMEPAD_MODE_OFF) {
        return 0;
    }

    len = (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) ? INTF_GAMEPAD_XBOX_RPT_LEN
                                                    : INTF_GAMEPAD_HID_RPT_LEN;
    if (buf_len < len) {
        return 0;
    }

    if (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) {
        (void)gamepad_encode_xbox(state, buf);
    } else {
        (void)gamepad_encode_hid(state, buf);
    }

    return len;
}

int hal_gamepad_send_raw(const uint8_t *report, uint16_t len, uint32_t timeout_ms)
{
    uint8_t ep_addr;

    if ((report == NULL) || (len == 0U) || (len > sizeof(s_tx_buf))) {
        return -1;
    }
    if ((g_gamepad_mode == HAL_GAMEPAD_MODE_OFF) || (s_tx_sem == NULL)) {
        return -2;
    }
    if (!usb_device_is_configured(CONFIG_USBD_BUSID)) {
        return -2;
    }

    ep_addr = (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) ? INTF_GAMEPAD_XBOX_EPI : INTF_GAMEPAD_HID_EPI;

    /* 丢弃上一次可能残留的完成信号，避免误判本次传输已完成 */
    while (usb_osal_sem_take(s_tx_sem, 0) == 0) {
    }

    memcpy(s_tx_buf, report, len);

    if (usbd_ep_start_write(CONFIG_USBD_BUSID, ep_addr, s_tx_buf, len) < 0) {
        return -3;
    }

    return (usb_osal_sem_take(s_tx_sem, timeout_ms) == 0) ? 0 : -4;
}

int hal_gamepad_send(const hal_gamepad_state_t *state, uint32_t timeout_ms)
{
    uint8_t report[INTF_GAMEPAD_XBOX_RPT_LEN];
    uint16_t len;

    if (state == NULL) {
        return -1;
    }
    if (g_gamepad_mode == HAL_GAMEPAD_MODE_OFF) {
        return -2;
    }

    if (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) {
        len = gamepad_encode_xbox(state, report);
    } else {
        len = gamepad_encode_hid(state, report);
    }

    return hal_gamepad_send_raw(report, len, timeout_ms);
}

int hal_gamepad_set_mode(gamepad_mode_ty mode)
{
    if ((mode != HAL_GAMEPAD_MODE_OFF) && (mode != HAL_GAMEPAD_MODE_HID) && (mode != HAL_GAMEPAD_MODE_XBOX)) {
        return -1;
    }
    if (mode == g_gamepad_mode) {
        return 0;
    }

    /* 先写模式，描述符回调在随后的重新枚举里读它 */
    g_gamepad_mode = mode;

    return hal_usb_reconfigure();
}

gamepad_mode_ty hal_gamepad_get_mode(void)
{
    return g_gamepad_mode;
}

int hal_gamepad_read_output(uint8_t *buf, uint16_t buf_len, uint16_t *out_len)
{
    size_t flag;
    uint32_t len;

    if ((buf == NULL) || (out_len == NULL) || (g_gamepad_mode != HAL_GAMEPAD_MODE_XBOX)) {
        return -1;
    }

    flag = usb_osal_enter_critical_section();
    if (s_out_seq == s_out_read_seq) {
        usb_osal_leave_critical_section(flag);
        return 0;
    }

    s_out_read_seq = s_out_seq;
    len = s_out_last_len;
    if (len > buf_len) {
        len = buf_len;
    }
    memcpy(buf, s_out_last, len);
    usb_osal_leave_critical_section(flag);

    *out_len = (uint16_t)len;
    return 1;
}

int hal_gamepad_get_rumble(uint8_t *left, uint8_t *right)
{
    if ((left == NULL) || (right == NULL)) {
        return -1;
    }
    if (g_gamepad_mode != HAL_GAMEPAD_MODE_XBOX) {
        return -1;
    }

    *left = s_rumble_left;
    *right = s_rumble_right;
    return 0;
}

/* 默认空实现，应用里定义同名函数即可覆盖 */
__attribute__((weak)) void hal_gamepad_rumble(uint8_t left, uint8_t right)
{
    (void)left;
    (void)right;
}
