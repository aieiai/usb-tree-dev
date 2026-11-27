#ifndef KEY_MOUSE_H
#define KEY_MOUSE_H

#include <stdint.h>
#include <stdbool.h>

#include "keycode.h"
#include "rotsta.h"

#define MS_RPT_PERIOD_US    (125U)

typedef struct {
    uint16_t v0;          /* 初始速度, 计数/s, [1, vmax] */
    uint16_t vmax;        /* 最大速度, 计数/s, [1, 15000] */
    uint16_t t_acc_ms;    /* 加速时间, ms, [50, 5000] */
    uint16_t wheel_rate;  /* 滚轮速率, 步/s, [1, 100] */
} keymouse_cfg_t;

/* 鼠标状态机：按钮持有计数、移动/滚轮通道、速度模型与上报节流。
 * 全部接口仅在 keycore 线程调用（与 keyhid 同线程）。 */
void keymouse_init(void);

/* 清空按钮/通道计数、速度与累积器状态（不动报告缓冲与速度参数）。 */
void keymouse_reset(void);

/* 鼠标键沿事件：kc 为按键按下/抬起时记录的鼠标键码。 */
void keymouse_press(uint8_t ki, const rotkc_t *kc);
void keymouse_release(uint8_t ki, const rotkc_t *kc);

/* 每帧调用一次；返回 true 表示到达上报时机且鼠标报告已被填好待发送。 */
bool keymouse_process(void);

/* 当前是否有鼠标键按住或待重发状态（挂起/复位时判断是否需要清主机侧状态）。 */
bool keymouse_active(void);

/* 发送结果回传：驱动失败重发与诊断日志（hal_kbd_send 返回值）。 */
void keymouse_tx_result(int result);

const keymouse_cfg_t *keymouse_cfg_get(void);
rotsta_t keymouse_cfg_set(const keymouse_cfg_t *cfg);

#endif  /* KEY_MOUSE_H */