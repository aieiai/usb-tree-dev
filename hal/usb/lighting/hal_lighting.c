#include "hal_lighting.h"
#include "../../hal_usb.h"
#include "../descriptor/hid_descriptor.h"

#include "usb_osal.h"
#include "usbd_core.h"
#include "usbd_hid.h"

extern const usb_descriptor_t g_desc_rpt_lighting;

static struct usbd_interface s_intf;

static void ep_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
}

static void hal_lighting_init(uint8_t busid)
{
    usbd_add_interface(busid, usbd_hid_init_intf(busid, &s_intf, g_desc_rpt_lighting.buf, g_desc_rpt_lighting.len));
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = INTF_LIGHTING_EPI, .ep_cb = ep_in_callback });
}

const hal_usb_fn_t hal_lighting_function = {
    .intf_num = INTF_LIGHTING_NUM,    
    .name = "lighting",
    .init = hal_lighting_init,
    .configured = NULL,
    .event = NULL,
};

#if HAL_LIGHTING_LAMPARRAY_ENABLE


/* 报告长度与 tu_hid_device.h 里的 TUD_HID_REPORT_DESC_LIGHTING 严格对应 */
#define LIGHTING_RPT_ATTR_LEN           (22U) /* LampCount(2) + 5 * u32            */
#define LIGHTING_RPT_ATTR_REQ_LEN       (2U)  /* LampId(2)                          */
#define LIGHTING_RPT_ATTR_RESP_LEN      (28U) /* LampId(2) + 5 * u32 + 6 * u8       */
#define LIGHTING_RPT_MULTI_UPDATE_LEN   (50U) /* 2 + 8 * u16 + 32 * u8              */
#define LIGHTING_RPT_RANGE_UPDATE_LEN   (9U)  /* 1 + 2 * u16 + 4 * u8               */
#define LIGHTING_RPT_CONTROL_LEN        (1U)  /* AutonomousMode(1)                  */

#define LAMP_ARRAY_KIND_KEYBOARD        (1U)
#define LAMP_MIN_UPDATE_INTERVAL_US     (5000U)
#define LAMP_LEVEL_COUNT                (255U)
#define LAMP_PURPOSE_CONTROL            (1U)
#define LAMP_MULTI_UPDATE_MAX           (8U)

/* 主机通过 Report 2 指定要读哪颗灯的属性 */
static uint16_t s_requested_lamp;
static uint8_t s_autonomous_mode;


static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

/* 简版布局：所有灯珠沿 X 轴等距排成一行，居中于各自的键位上 */
static uint32_t lamp_pos_x_um(uint16_t lamp_id)
{
    return ((uint32_t)lamp_id * HAL_LIGHTING_LAMP_PITCH_UM) + (HAL_LIGHTING_LAMP_PITCH_UM / 2U);
}

/* 覆盖 cherryusb 的 __WEAK 实现：除灯效接口外保持原来的默认行为 */
void usbd_hid_get_report(uint8_t busid, uint8_t intf, uint8_t report_id, uint8_t report_type, uint8_t **data, uint32_t *len)
{
    uint8_t *out = *data;

    (void)busid;
    (void)report_type;

    if (intf != INTF_LIGHTING_NUM) {
        out[0] = 0;
        *len = 1;
        return;
    }

    switch (report_id) {
    case LIGHTING_REPORT_ID_ATTRIBUTES:
        put_u16(&out[0], HAL_LIGHTING_LAMP_COUNT);
        put_u32(&out[2], (uint32_t)HAL_LIGHTING_LAMP_COUNT * HAL_LIGHTING_LAMP_PITCH_UM);
        put_u32(&out[6], HAL_LIGHTING_LAMP_SIZE_UM);
        put_u32(&out[10], HAL_LIGHTING_LAMP_DEPTH_UM);
        put_u32(&out[14], LAMP_ARRAY_KIND_KEYBOARD);
        put_u32(&out[18], LAMP_MIN_UPDATE_INTERVAL_US);
        *len = LIGHTING_RPT_ATTR_LEN;
        break;

    case LIGHTING_REPORT_ID_ATTR_RESPONSE: {
        uint16_t id = s_requested_lamp;

        if (id >= HAL_LIGHTING_LAMP_COUNT) {
            id = 0;
        }
        put_u16(&out[0], id);
        put_u32(&out[2], lamp_pos_x_um(id));
        put_u32(&out[6], HAL_LIGHTING_LAMP_SIZE_UM / 2U);
        put_u32(&out[10], 0);
        put_u32(&out[14], 0);                 /* UpdateLatencyInMicroseconds */
        put_u32(&out[18], LAMP_PURPOSE_CONTROL);
        out[22] = (uint8_t)LAMP_LEVEL_COUNT;  /* Red / Green / Blue / Intensity */
        out[23] = (uint8_t)LAMP_LEVEL_COUNT;
        out[24] = (uint8_t)LAMP_LEVEL_COUNT;
        out[25] = (uint8_t)LAMP_LEVEL_COUNT;
        out[26] = 1;                          /* IsProgrammable */
        out[27] = 0;                          /* InputBinding */
        *len = LIGHTING_RPT_ATTR_RESP_LEN;
        break;
    }

    case LIGHTING_REPORT_ID_CONTROL:
        out[0] = s_autonomous_mode;
        *len = LIGHTING_RPT_CONTROL_LEN;
        break;

    default:
        memset(out, 0, 8);
        *len = 1;
        break;
    }
}

void usbd_hid_set_report(uint8_t busid, uint8_t intf, uint8_t report_id, uint8_t report_type, uint8_t *report, uint32_t report_len)
{
    (void)busid;
    (void)report_type;

    if (intf != INTF_LIGHTING_NUM) {
        return;
    }

    switch (report_id) {
    case LIGHTING_REPORT_ID_ATTR_REQUEST:
        if (report_len >= LIGHTING_RPT_ATTR_REQ_LEN) {
            s_requested_lamp = get_u16(report);
        }
        break;

    case LIGHTING_REPORT_ID_MULTI_UPDATE: {
        uint32_t i;
        uint32_t n;

        if (report_len < LIGHTING_RPT_MULTI_UPDATE_LEN) {
            break;
        }
        n = report[0]; /* LampCount */
        if (n > LAMP_MULTI_UPDATE_MAX) {
            n = LAMP_MULTI_UPDATE_MAX;
        }
        for (i = 0; i < n; i++) {
            uint16_t id = get_u16(&report[2U + (2U * i)]);
            const uint8_t *ch = &report[18U + (4U * i)];

            if (id < HAL_LIGHTING_LAMP_COUNT) {
                hal_lighting_set_color(id, ch[0], ch[1], ch[2], ch[3]);
            }
        }
        break;
    }

    case LIGHTING_REPORT_ID_RANGE_UPDATE: {
        uint16_t id;
        uint16_t start;
        uint16_t end;

        if (report_len < LIGHTING_RPT_RANGE_UPDATE_LEN) {
            break;
        }
        start = get_u16(&report[1]);
        end = get_u16(&report[3]);
        for (id = start; (id <= end) && (id < HAL_LIGHTING_LAMP_COUNT); id++) {
            hal_lighting_set_color(id, report[5], report[6], report[7], report[8]);
        }
        break;
    }

    case LIGHTING_REPORT_ID_CONTROL:
        if (report_len >= LIGHTING_RPT_CONTROL_LEN) {
            s_autonomous_mode = report[0];
        }
        break;

    default:
        break;
    }
}

#endif /* HAL_LIGHTING_LAMPARRAY_ENABLE */

/* 默认空实现，应用里定义同名函数即可覆盖 */
__attribute__((weak)) void hal_lighting_set_color(uint16_t lamp_id, uint8_t red, uint8_t green, uint8_t blue, uint8_t intensity)
{
    (void)lamp_id;
    (void)red;
    (void)green;
    (void)blue;
    (void)intensity;
}
