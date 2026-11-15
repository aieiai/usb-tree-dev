#ifndef HAL_GAMEPAD_H
#define HAL_GAMEPAD_H

#include <stdint.h>
#include <string.h>


/* 游戏手柄接口（interface 4）：
 *  - HID 模式：标准 HID Gamepad 报告描述符，免驱，IN 报告 13 字节；
 *  - Xbox 模式：vendor class 0xFF/0x5D/0x01 + Xbox 私有描述符，IN 报告 20 字节，
 *                OUT 端点用于接收主机下发的震动数据。MS OS 2.0 里带 XUSB10 兼容 ID，
 *                Windows 会用 xusb22.sys 加载该接口，VID/PID 仍可保持自定义值。
 * 两种模式共用同一份 hal_gamepad_state_t，由本模块负责编码。 */

/* 按键位图，顺序与 HID 报告描述符的 16 个 button usage 一一对应
 * （bit0..10 与已验证的 RAY40 工程保持一致，bit11..15 保留） */
#define HAL_GAMEPAD_BTN_A           (1U << 0)
#define HAL_GAMEPAD_BTN_B           (1U << 1)
#define HAL_GAMEPAD_BTN_X           (1U << 2)
#define HAL_GAMEPAD_BTN_Y           (1U << 3)
#define HAL_GAMEPAD_BTN_LB          (1U << 4)
#define HAL_GAMEPAD_BTN_RB          (1U << 5)
#define HAL_GAMEPAD_BTN_BACK        (1U << 6)
#define HAL_GAMEPAD_BTN_START       (1U << 7)
#define HAL_GAMEPAD_BTN_GUIDE       (1U << 8)
#define HAL_GAMEPAD_BTN_L3          (1U << 9)
#define HAL_GAMEPAD_BTN_R3          (1U << 10)
#define HAL_GAMEPAD_BTN_SHARE       (1U << 11) /* Xbox 模式无对应键，HID 模式可用 */

/* 方向帽：0 起顺时针，与 HID hat switch 取值一致 */
#define HAL_GAMEPAD_HAT_UP          (0U)
#define HAL_GAMEPAD_HAT_UP_RIGHT    (1U)
#define HAL_GAMEPAD_HAT_RIGHT       (2U)
#define HAL_GAMEPAD_HAT_DOWN_RIGHT  (3U)
#define HAL_GAMEPAD_HAT_DOWN        (4U)
#define HAL_GAMEPAD_HAT_DOWN_LEFT   (5U)
#define HAL_GAMEPAD_HAT_LEFT        (6U)
#define HAL_GAMEPAD_HAT_UP_LEFT     (7U)
#define HAL_GAMEPAD_HAT_NONE        (8U)    /* 居中：超出 0..7 的合法范围 */

typedef struct {
    uint16_t buttons;   /* HAL_GAMEPAD_BTN_* 位组合 */
    uint8_t  hat;       /* HAL_GAMEPAD_HAT_*，居中填 HAL_GAMEPAD_HAT_NONE */
    int16_t  lx;        /* 左摇杆 X，-32767(左) .. 32767(右) */
    int16_t  ly;        /* 左摇杆 Y，-32767(下) .. 32767(上) */
    int16_t  rx;        /* 右摇杆 X，-32767(左) .. 32767(右) */
    int16_t  ry;        /* 右摇杆 Y，-32767(下) .. 32767(上) */
    uint8_t  lt;        /* 左扳机，0 .. 255 */
    uint8_t  rt;        /* 右扳机，0 .. 255 */
} hal_gamepad_state_t;

/* ---- 两种模式在线路上的报告布局 ----
 * 下面的结构体只用于对照布局 / 调试打印，实际收发请用 hal_gamepad_send()：
 * 结构体是 packed 的，lx 等成员落在奇数偏移上，直接读写会触发非对齐访问。
 *
 * HID 模式：独立接口、无 Report ID、13 字节
 *   off  size  字段
 *   0    2     buttons  bit0=A bit1=B bit2=X bit3=Y bit4=LB bit5=RB
 *                      bit6=Back bit7=Start bit8=Guide bit9=L3 bit10=R3 bit11=Share
 *                      （bit12..15 保留）
 *   2    1     hat      低 4 位 0..7，8=居中；高 4 位是描述符里的 padding，恒 0
 *   3    2     lx       int16 LE（HID X 轴）
 *   5    2     ly       int16 LE（HID Y 轴；正=上，编码时按 HID 约定取反）
 *   7    2     rx       int16 LE（HID Z 轴，右摇杆 X）
 *   9    2     ry       int16 LE（HID Rx 轴，右摇杆 Y）
 *   11   1     lt       uint8   （HID Ry 轴）
 *   12   1     rt       uint8   （HID Rz 轴）
 *
 *   注：Chrome 在 Windows 上走 DirectInput 时，axes[] 按 X,Y,Z,Rx,Ry,Rz 取值
 *   （对应 DIJOYSTATE2 的 lX,lY,lZ,lRx,lRy,lRz），不是 Gamepad 规范里的
 *   X,Y,Z,Rz。因此右摇杆 Y 必须占 Rx、扳机排到 Ry/Rz；否则 axes[3] 会读到
 *   Rx（扳机=0），表现为右摇杆 Y 顶在最小值。
 *
 * XBOX 模式：vendor 接口、20 字节
 *   off  size  字段
 *   0    1     type     固定 0x00
 *   1    1     size     固定 0x14（20）
 *   2    2     buttons  16 位位图 LE：bit0=上 bit1=下 bit2=左 bit3=右
 *                      bit4=Start bit5=Back bit6=L3 bit7=R3 bit8=LB bit9=RB
 *                      bit10=Guide bit11=保留 bit12=A bit13=B bit14=X bit15=Y
 *                      （与 HID 的位序不同，见 hal_gamepad.c 的编码函数）
 *   4    1     lt       uint8
 *   5    1     rt       uint8
 *   6    2     lx       int16 LE
 *   8    2     ly       int16 LE
 *   10   2     rx       int16 LE
 *   12   2     ry       int16 LE
 *   14   6     reserved 全 0
 */
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    uint8_t  hat;
    int16_t  lx;
    int16_t  ly;
    int16_t  rx;
    int16_t  ry;
    uint8_t  lt;
    uint8_t  rt;
} hal_gamepad_hid_report_t;   /* 13 字节 */

typedef struct __attribute__((packed)) {
    uint8_t  type;            /* 0x00 */
    uint8_t  size;            /* 0x14 */
    uint16_t buttons;
    uint8_t  lt;
    uint8_t  rt;
    int16_t  lx;
    int16_t  ly;
    int16_t  rx;
    int16_t  ry;
    uint8_t  reserved[6];
} hal_gamepad_xbox_report_t;  /* 20 字节 */

/* XBOX 模式 OUT 报告（主机 → 设备，8 字节）
 *   off  size  字段
 *   0    1     type         0x00 = 震动包；0x01 = LED 包（部分驱动会发）
 *   1    1     size         0x08
 *   2    1     reserved     0x00
 *   3    1     big_motor    左（大）马达 0..255
 *   4    1     small_motor  右（小）马达 0..255
 *   5    3     reserved     全 0
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* 0x00 */
    uint8_t  size;            /* 0x08 */
    uint8_t  reserved;
    uint8_t  big_motor;       /* 左马达 */
    uint8_t  small_motor;     /* 右马达 */
    uint8_t  reserved2[3];
} hal_gamepad_xbox_out_report_t;  /* 8 字节 */

/* 主机下发的震动强度（仅 Xbox 模式有 OUT 端点）。
 * 默认空实现，应用里定义同名函数即可覆盖。 */
void hal_gamepad_rumble(uint8_t left, uint8_t right);

/* 编码并发送一帧手柄状态，阻塞等待传输完成。
 * 返回 0 表示成功；-1 参数错误，-2 未启用手柄 / 主机未配置，-3 端点忙，-4 等待超时。 */
int hal_gamepad_send(const hal_gamepad_state_t *state, uint32_t timeout_ms);

/* 直接发送已编码的报告（长度需与当前模式匹配：HID 13 字节，Xbox 20 字节）。 */
int hal_gamepad_send_raw(const uint8_t *report, uint16_t len, uint32_t timeout_ms);

/* 把状态编码成当前模式的报告但不发送，返回字节数（HID 13 / Xbox 20）；
 * buf 不够长或手柄为 OFF 时返回 0。用于调试打印 / 自检对照。 */
uint16_t hal_gamepad_encode(const hal_gamepad_state_t *state, uint8_t *buf, uint16_t buf_len);

/* 读取最近一次收到的 Xbox OUT 报告（主机 → 设备）。
 * 返回 1 = 取到新报告（buf 至少 8 字节，*out_len 为实际长度），
 *      0 = 自上次读取后没有新数据，
 *     -1 = 当前不是 Xbox 模式（HID 模式没有 OUT 端点）或参数非法。
 * 用于调试打印 / 应用侧处理震动、LED；中断里只做缓存，实际解析放任务上下文。 */
int hal_gamepad_read_output(uint8_t *buf, uint16_t buf_len, uint16_t *out_len);


/* 读取最近一次收到的震动强度，返回 0 表示有效，-1 表示当前模式没有震动通道。 */
int hal_gamepad_get_rumble(uint8_t *left, uint8_t *right);

#endif
