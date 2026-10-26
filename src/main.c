
#include "tx_api.h"
#include "tx_task_config.h"

#include "hal_usb.h"
#include "hal_kbd.h"
#include "hal_dt.h"
#include "hal_lighting.h"
#include "hal_gamepad.h"

#include "usb_osal.h"
#include "usb_config.h"

#include "board.h"
#include "hpm_gpio_drv.h"


#define task1_PRIORITY          (TX_MAX_PRIORITIES - 5U)
#define TX_MEM_POOL_SIZE        (16U * 1024U)
#define LED_FLASH_PERIOD_IN_MS  (300)

/* 演示/自检日志：量产时改为 0 或直接删掉演示代码，DT 接口本身不受影响 */
#ifndef APP_LOG_DEMO
#define APP_LOG_DEMO           1
#endif

/* 手柄演示：1 = 启动即以 Xbox 模式枚举，并创建一个 4ms 周期的手柄上报任务。
 * 该任务做逐位自检：每个按键 / 方向 / 摇杆轴 / 扳机依次单独动作一遍，
 * 一轮 27 项 ≈ 27 秒，循环播放；当前项通过 DT 通道打印。
 * 接入真实输入后删掉这个任务即可。 */
#ifndef APP_GAMEPAD_DEMO
#define APP_GAMEPAD_DEMO      0
#endif

#if APP_GAMEPAD_DEMO
#define GAMEPAD_TASK_PRIORITY      (TX_MAX_PRIORITIES - 6U)
#define GAMEPAD_TASK_STACK         (1024U)
/* Xbox 主机按 4ms 轮询，上报周期必须跟得上；HID 模式同样适用 */
#define GAMEPAD_REPORT_PERIOD_MS   (4U)
/* 每个测试项持续 250 帧 ≈ 1 秒 */
#define GAMEPAD_TEST_FRAMES        (250U)

/* 每个测试项：按键掩码 + 名称 + 在两种报告里的位置。
 * 位置写法 "HID bX / XBOX bY"，bX 是按键位图里的位号（bitX）：
 *   HID  报告 13B：buttons -> byte0..1（b0..b15）
 *   XBOX 报告 20B：buttons -> byte2..3（b0..b15，位序与 HID 不同）
 * 摇杆 / 扳机直接标报告字节偏移和位宽。 */
typedef struct {
    uint16_t mask;
    const char *name;
    const char *pos;
} gp_test_button_t;

static const gp_test_button_t s_gp_buttons[] = {
    { HAL_GAMEPAD_BTN_A,     "A",     "HID b0,  XBOX b12" },
    { HAL_GAMEPAD_BTN_B,     "B",     "HID b1,  XBOX b13" },
    { HAL_GAMEPAD_BTN_X,     "X",     "HID b2,  XBOX b14" },
    { HAL_GAMEPAD_BTN_Y,     "Y",     "HID b3,  XBOX b15" },
    { HAL_GAMEPAD_BTN_LB,    "LB",    "HID b4,  XBOX b8"  },
    { HAL_GAMEPAD_BTN_RB,    "RB",    "HID b5,  XBOX b9"  },
    { HAL_GAMEPAD_BTN_BACK,  "Back",  "HID b6,  XBOX b5"  },
    { HAL_GAMEPAD_BTN_START, "Start", "HID b7,  XBOX b4"  },
    { HAL_GAMEPAD_BTN_GUIDE, "Guide", "HID b8,  XBOX b10" },
    { HAL_GAMEPAD_BTN_L3,    "L3",    "HID b9,  XBOX b6"  },
    { HAL_GAMEPAD_BTN_R3,    "R3",    "HID b10, XBOX b7"  },
    { HAL_GAMEPAD_BTN_SHARE, "Share", "HID b11, XBOX n/a" },
};
#define GAMEPAD_BTN_COUNT  (sizeof(s_gp_buttons) / sizeof(s_gp_buttons[0]))
/* 一轮：12 键 + 8 方向 + 7 个轴/扳机/基线项 */
#define GAMEPAD_STAGE_COUNT (GAMEPAD_BTN_COUNT + 8U + 7U)

static const char *const s_gp_hat_names[8] = {
    "D-pad up",   "D-pad up-right", "D-pad right", "D-pad down-right",
    "D-pad down", "D-pad down-left", "D-pad left", "D-pad up-left",
};
/* 方向帽：HID 是 byte2 的低 4 位数值；XBOX 是 byte2 的方向位组合 */
static const char *const s_gp_hat_pos[8] = {
    "HID hat=0, XBOX b0",    "HID hat=1, XBOX b0+b3",
    "HID hat=2, XBOX b3",    "HID hat=3, XBOX b1+b3",
    "HID hat=4, XBOX b1",    "HID hat=5, XBOX b1+b2",
    "HID hat=6, XBOX b2",    "HID hat=7, XBOX b0+b2",
};

/* 把编码后的报告转成 "xx xx ..." 字符串（仅调试打印用）
 * 缓冲按 OUT 端点的最大包长算，IN 报告（HID 13 / XBOX 20）和 OUT 帧共用都不越界。 */
static const char *gamepad_hex_str(const uint8_t *buf, uint16_t len)
{
    static char s_hex[INTF_GAMEPAD_XBOX_EPO_SIZE * 3U + 1U];
    static const char s_digits[] = "0123456789abcdef";
    uint16_t i;

    if (len > INTF_GAMEPAD_XBOX_EPO_SIZE) {
        len = INTF_GAMEPAD_XBOX_EPO_SIZE;
    }

    for (i = 0; i < len; i++) {
        s_hex[i * 3U]      = s_digits[buf[i] >> 4];
        s_hex[i * 3U + 1U] = s_digits[buf[i] & 0x0FU];
        s_hex[i * 3U + 2U] = ' ';
    }
    s_hex[len * 3U] = '\0';

    return s_hex;
}

/* 生成第 n 帧的自检内容；阶段首帧顺便打印当前测试项及其位号。
 * 每帧先全部复位，保证同一时刻只有一个输入量在动，便于逐位判断。 */
static void gamepad_selftest_frame(uint32_t n, hal_gamepad_state_t *gp)
{
    uint32_t stage = (n / GAMEPAD_TEST_FRAMES) % GAMEPAD_STAGE_COUNT;
    uint32_t phase = n % GAMEPAD_TEST_FRAMES;
    const char *name;
    const char *pos;
    int16_t sweep;
    uint8_t trig;

    gp->buttons = 0;
    gp->hat     = HAL_GAMEPAD_HAT_NONE;
    gp->lx      = 0;
    gp->ly      = 0;
    gp->rx      = 0;
    gp->ry      = 0;
    gp->lt      = 0;
    gp->rt      = 0;

    if (stage < GAMEPAD_BTN_COUNT) {
        gp->buttons = s_gp_buttons[stage].mask;
        name = s_gp_buttons[stage].name;
        pos  = s_gp_buttons[stage].pos;
    } else if (stage < GAMEPAD_BTN_COUNT + 8U) {
        gp->hat = (uint8_t)(stage - GAMEPAD_BTN_COUNT);
        name = s_gp_hat_names[gp->hat];
        pos  = s_gp_hat_pos[gp->hat];
    } else {
        /* 轴：-32767 → +32767；扳机：0 → 255，都在 1 秒内扫完 */
        sweep = (int16_t)(((int32_t)phase * 65534) / (int32_t)(GAMEPAD_TEST_FRAMES - 1U) - 32767);
        trig  = (uint8_t)((phase * 255U) / (GAMEPAD_TEST_FRAMES - 1U));

        switch ((stage - GAMEPAD_BTN_COUNT - 8U) % 7U) {
        case 0:
            gp->lx = sweep;
            name = "Left stick X";
            pos  = "HID X byte3..4 i16, XBOX byte6..7 i16";
            break;
        case 1:
            gp->ly = sweep;
            name = "Left stick Y";
            pos  = "HID Y byte5..6 i16, XBOX byte8..9 i16";
            break;
        case 2:
            gp->rx = sweep;
            name = "Right stick X";
            pos  = "HID Z byte7..8 i16, XBOX byte10..11 i16";
            break;
        case 3:
            gp->ry = sweep;
            name = "Right stick Y";
            pos  = "HID Rx byte9..10 i16, XBOX byte12..13 i16";
            break;
        case 4:
            gp->lt = trig;
            name = "Left trigger";
            pos  = "HID Ry byte11 u8, XBOX byte4 u8";
            break;
        case 5:
            gp->rt = trig;
            name = "Right trigger";
            pos  = "HID Rz byte12 u8, XBOX byte5 u8";
            break;
        default:
            name = "All released (baseline)";
            pos  = "all zero";
            break;
        }
    }

    if (phase == 0U) {
        uint8_t frame[INTF_GAMEPAD_XBOX_RPT_LEN];
        uint16_t len = hal_gamepad_encode(gp, frame, sizeof(frame));

        hal_dt_printf("[gp] test: %-22s [%s]\r\n", name, pos);
        hal_dt_printf("[gp] %s %uB: %s\r\n",
                      (g_gamepad_mode == HAL_GAMEPAD_MODE_XBOX) ? "XBOX" : "HID",
                      (unsigned)len, gamepad_hex_str(frame, len));
    }
}

/* 打印主机下发的 Xbox OUT 报告（原始 hex + 解析后的震动强度）。
 * OUT 回调在中断里只做缓存，这里在任务上下文解析，避免在 ISR 里跑 vsnprintf。 */
static void gamepad_log_output(void)
{
    uint8_t buf[INTF_GAMEPAD_XBOX_EPO_SIZE];
    hal_gamepad_xbox_out_report_t rpt;
    uint16_t len = 0;
    uint16_t show;

    if (hal_gamepad_read_output(buf, sizeof(buf), &len) != 1) {
        return;
    }

    /* 每行最多打 16 字节，避免超过 DT 日志 128 字节的单条上限 */
    show = (len > 16U) ? 16U : len;

    if (len >= (uint16_t)sizeof(rpt)) {
        memcpy(&rpt, buf, sizeof(rpt));
        hal_dt_printf("[gp] OUT %uB: %s| type=0x%02x size=0x%02x motor L=%u R=%u\r\n",
                      (unsigned)len, gamepad_hex_str(buf, show),
                      (unsigned)rpt.type, (unsigned)rpt.size,
                      (unsigned)rpt.big_motor, (unsigned)rpt.small_motor);
    } else {
        hal_dt_printf("[gp] OUT %uB: %s| short frame\r\n",
                      (unsigned)len, gamepad_hex_str(buf, show));
    }
}

static void gamepad_demo_task(void *pvParameters)
{
    hal_gamepad_state_t gp = { .hat = HAL_GAMEPAD_HAT_NONE };
    uint32_t n = 0;

    (void)pvParameters;

    hal_dt_puts("[gp] selftest: 27 items x 1s, loop. b0..b15 = bit index in buttons bitmap\r\n");
    hal_dt_puts("[gp] HID rpt 13B: buttons byte0-1, hat byte2, axes byte3-10, trig byte11-12\r\n");
    hal_dt_puts("[gp] XBOX rpt 20B: buttons byte2-3, trig byte4-5, axes byte6-13\r\n");
    hal_dt_puts("[gp] XBOX OUT 8B: type size rsvd motorL motorR rsvd[3]\r\n");
    hal_dt_puts("[gp] each item also prints the encoded report hex\r\n");

    while (1) {
        gamepad_selftest_frame(n, &gp);
        (void)hal_gamepad_send(&gp, 10U);
        gamepad_log_output();   /* Xbox 模式：打印主机下发的 OUT 报告 */
        n++;
        usb_osal_msleep(GAMEPAD_REPORT_PERIOD_MS);
    }
}
#endif


TX_BYTE_POOL tx_byte_pool;
static UCHAR tx_memory_area[TX_MEM_POOL_SIZE] __attribute__((aligned(8)));


extern void webusb_hid_keyboard_init(uint8_t busid, uintptr_t reg_base);
extern void gamepad_init(uint8_t busid, uintptr_t reg_base);
extern void gamepad_test(uint8_t busid);

#if APP_LOG_DEMO
/* 演示用：把 Windows 下发的第 0 颗灯颜色记下来，在诊断数据里显示，便于确认灯效数据到了 */
static volatile uint8_t s_lamp0[4];

void hal_lighting_set_color(uint16_t lamp_id, uint8_t red, uint8_t green, uint8_t blue, uint8_t intensity)
{
    if (lamp_id == 0U) {
        s_lamp0[0] = red;
        s_lamp0[1] = green;
        s_lamp0[2] = blue;
        s_lamp0[3] = intensity;
    }
}
#endif

static void task1(void *pvParameters)
{
    (void)pvParameters;
    uint32_t tick = 0;
    uint8_t rx[64];

#if APP_LOG_DEMO
    printf("[cherryusb hid keyboard with freertos sample]: task started.\n");
    hal_dt_puts("[ray] diagnostic tracking channel ready\r\n");
#endif

    while (1) {
#if APP_LOG_DEMO
        int n;

        /* DT OUT 端点：上位机下发的数据，这里回显一下，方便验证双向通道 */
        n = hal_dt_read(rx, sizeof(rx), 0);
        if (n > 0) {
            hal_dt_printf("[ray] host -> dev, %d bytes\r\n", n);
        }

        /* kbd_led 反映键盘 LED 输出报告；lamp0 反映 Windows 下发的灯效颜色 */
        hal_dt_printf("[ray] alive, tick=%u, dropped=%u, kbd_led=0x%02x, lamp0=%02x%02x%02x\r\n",
                      (unsigned)tick++, (unsigned)hal_dt_dropped(), hal_kbd_get_leds(),
                      s_lamp0[0], s_lamp0[1], s_lamp0[2]);
#else
        (void)tick;
        (void)rx;
#endif

        /* 手柄上报由 gamepad_demo_task 以 4ms 周期负责 */
        // gamepad_test(CONFIG_USBD_BUSID);
        usb_osal_msleep(1000);
    }
}

/* Define what the initial system looks like.  */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    
    tx_byte_pool_create(&tx_byte_pool, "tx byte pool", tx_memory_area, TX_MEM_POOL_SIZE);

#if APP_GAMEPAD_DEMO
    /* 首轮枚举就是 HID 手柄模式，直接写模式变量即可，不需要重新枚举 */
    g_gamepad_mode = HAL_GAMEPAD_MODE_XBOX;
#endif

    hal_usb_init();
    
    // webusb_hid_keyboard_init(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE);
    // gamepad_init(CONFIG_USBD_BUSID, CONFIG_USBD_REG_BASE);


    if (usb_osal_thread_create("task1", 8192U, task1_PRIORITY, task1, NULL) == NULL) {
        perror("Task1 creation failed!.\n");
        for (;;) {
            ;
        }
    }

#if APP_GAMEPAD_DEMO
    if (usb_osal_thread_create("gamepad", GAMEPAD_TASK_STACK, GAMEPAD_TASK_PRIORITY,
                               gamepad_demo_task, NULL) == NULL) {
        perror("Gamepad demo task creation failed!.\n");
    }
#endif
}
int main(void)
{
    board_init();
    board_init_led_pins();
    board_init_gpio_pins();
    gpio_set_pin_input(BOARD_APP_GPIO_CTRL, BOARD_APP_GPIO_INDEX, BOARD_APP_GPIO_PIN);

    board_timer_create(LED_FLASH_PERIOD_IN_MS, board_led_toggle);

    board_init_usb((USB_Type *)CONFIG_USBD_REG_BASE);
    intc_set_irq_priority(CONFIG_HPM_USBD_IRQn, 2);

    printf("cherry usb threadx hid_keyboard device sample.\n");

    /* Enter the ThreadX kernel.  */
    tx_kernel_enter();

    for (;;) {
        ;
    }
    return 0;
}

