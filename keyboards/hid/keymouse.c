/**
 * @Brief:  mouse key state machine: button/motion hold counts, speed model and report cadence.
 */
#include "keymouse.h"
#include "keyrpt.h"

#include "usb/diagnostic_tracking/hal_dt.h"

#include <string.h>

#define TAG "keymouse: "

#define MS_FRAME_US             (125U)
#define MS_FRAMES_PER_MS        (1000U / MS_FRAME_US)
#define MS_RPT_PERIOD_FRAMES    (MS_RPT_PERIOD_US / MS_FRAME_US)
#define MS_AXIS_MAX_OUTPUT      (120)
#define MS_AXIS_Q16_ONE         (65536U)

_Static_assert((MS_RPT_PERIOD_US % MS_FRAME_US) == 0U,
               "MS_RPT_PERIOD_US must be a multiple of the HS frame interval");

#define MS_BTN_NUM              (8U)
#define MS_CH_NUM               (6U)
#define MS_AXIS_NUM             (3U)

typedef struct {
    uint8_t  cnt;         /* 同功能按键持有计数 */
    uint32_t start_frame; /* 激活起始帧（以 tick 为基准） */
} keymouse_ch_t;

typedef struct {
    int32_t accum; /* Q16.16 位移累积器（消余数漂移） */
    int16_t out;   /* 本周期输出 */
} keymouse_axis_t;

typedef struct {
    uint8_t  btn_cnt[MS_BTN_NUM];
    keymouse_ch_t ch[MS_CH_NUM];
    keymouse_axis_t axis[MS_AXIS_NUM];
    keymouse_cfg_t cfg;
    uint32_t frame_cnt; /* 节流相位计数 */
    uint32_t tick;      /* 单调帧计数（速度计时基准，不重置） */
    uint8_t  immediate : 1;
    uint8_t  pending : 1;
    uint8_t  fail_cnt;
} keymouse_t;

static keymouse_t s_ms;

/* 按键位 MS_BTNL..MS_BTN3 与 HID 报告按钮位一一对应（bit0 = 按钮 1）。
 * 按位增减持有计数：同一按钮被多个物理键映射时，全部抬起才清除报告位。 */
static bool _btn_apply(uint8_t mask, bool press)
{
    bool changed = false;

    for (uint8_t i = 0U; i < MS_BTN_NUM; i++) {
        uint8_t bit = (uint8_t)(1U << i);

        if ((mask & bit) == 0U) {
            continue;
        }
        if (press) {
            if (s_ms.btn_cnt[i] == 0U) {
                keyrpt_ms_add_btn(bit);
                changed = true;
            }
            if (s_ms.btn_cnt[i] < 0xFFU) {
                s_ms.btn_cnt[i]++;
            }
        }
        else if (s_ms.btn_cnt[i] == 0U) {
            hal_dt_printf(TAG "release underflow btn=%u\r\n", (unsigned)i);
        }
        else {
            s_ms.btn_cnt[i]--;
            if (s_ms.btn_cnt[i] == 0U) {
                keyrpt_ms_del_btn(bit);
                changed = true;
            }
        }
    }
    return changed;
}

/* 移动/滚轮通道下标：X+/X-/Y+/Y-/W+/W-，同轴正反为相邻通道 */
static int8_t _ch_index(uint8_t sy)
{
    if ((sy < MS_XP) || (sy > MS_WN)) {
        return -1;
    }
    return (int8_t)(sy - MS_XP);
}

static bool _any_active(void)
{
    for (uint8_t i = 0U; i < MS_BTN_NUM; i++) {
        if (s_ms.btn_cnt[i] != 0U) {
            return true;
        }
    }
    for (uint8_t i = 0U; i < MS_CH_NUM; i++) {
        if (s_ms.ch[i].cnt != 0U) {
            return true;
        }
    }
    return false;
}

/* 通道激活后第 t 毫秒的速度：v0 + (vmax - v0) * t / t_acc，到 t_acc 后恒为 vmax */
static uint32_t _ch_speed(const keymouse_ch_t *ch)
{
    uint32_t elapsed_ms = (uint32_t)(s_ms.tick - ch->start_frame) / MS_FRAMES_PER_MS;
    const keymouse_cfg_t *cfg = &s_ms.cfg;

    if (elapsed_ms >= (uint32_t)cfg->t_acc_ms) {
        return cfg->vmax;
    }
    return (uint32_t)cfg->v0 +
           (((uint32_t)cfg->vmax - (uint32_t)cfg->v0) * elapsed_ms) / (uint32_t)cfg->t_acc_ms;
}

static int16_t _axis_output(uint8_t axis)
{
    uint8_t pos_ch = (uint8_t)(axis * 2U);
    uint8_t neg_ch = (uint8_t)(pos_ch + 1U);
    bool pos_act = (s_ms.ch[pos_ch].cnt != 0U);
    bool neg_act = (s_ms.ch[neg_ch].cnt != 0U);
    uint32_t dq16;
    int32_t accum;
    int32_t out;

    s_ms.axis[axis].out = 0;
    if (pos_act == neg_act) {
        return 0; /* 都未按下，或同轴正反同时按住（互斥输出为零） */
    }

    dq16 = (uint32_t)(((uint64_t)_ch_speed(pos_act ? &s_ms.ch[pos_ch] : &s_ms.ch[neg_ch]) *
                       MS_RPT_PERIOD_US * MS_AXIS_Q16_ONE) / 1000000U);

    accum = s_ms.axis[axis].accum + (pos_act ? (int32_t)dq16 : -(int32_t)dq16);
    out = accum / (int32_t)MS_AXIS_Q16_ONE;
    accum -= out * (int32_t)MS_AXIS_Q16_ONE;

    if (out > MS_AXIS_MAX_OUTPUT) {
        out = MS_AXIS_MAX_OUTPUT;
    }
    else if (out < -MS_AXIS_MAX_OUTPUT) {
        out = -MS_AXIS_MAX_OUTPUT;
    }

    s_ms.axis[axis].accum = accum;
    s_ms.axis[axis].out = (int16_t)out;
    return (int16_t)out;
}

/* 滚轮轴：固定速率，不做加速 */
static int16_t _wheel_output(void)
{
    const uint8_t pos_ch = 4U;
    const uint8_t neg_ch = 5U;
    bool pos_act = (s_ms.ch[pos_ch].cnt != 0U);
    bool neg_act = (s_ms.ch[neg_ch].cnt != 0U);
    uint32_t dq16;
    int32_t accum;
    int32_t out;

    s_ms.axis[2].out = 0;
    if (pos_act == neg_act) {
        return 0;
    }

    dq16 = (uint32_t)(((uint64_t)s_ms.cfg.wheel_rate * MS_RPT_PERIOD_US * MS_AXIS_Q16_ONE) / 1000000U);

    accum = s_ms.axis[2].accum + (pos_act ? (int32_t)dq16 : -(int32_t)dq16);
    out = accum / (int32_t)MS_AXIS_Q16_ONE;
    accum -= out * (int32_t)MS_AXIS_Q16_ONE;

    if (out > MS_AXIS_MAX_OUTPUT) {
        out = MS_AXIS_MAX_OUTPUT;
    }
    else if (out < -MS_AXIS_MAX_OUTPUT) {
        out = -MS_AXIS_MAX_OUTPUT;
    }

    s_ms.axis[2].accum = accum;
    s_ms.axis[2].out = (int16_t)out;
    return (int16_t)out;
}

static void _axis_clear_if_idle(uint8_t axis)
{
    uint8_t pos_ch = (uint8_t)(axis * 2U);

    if ((s_ms.ch[pos_ch].cnt == 0U) && (s_ms.ch[pos_ch + 1U].cnt == 0U)) {
        s_ms.axis[axis].accum = 0;
        s_ms.axis[axis].out = 0;
    }
}

void keymouse_init(void)
{
    keymouse_reset();
    s_ms.cfg.v0 = 80U;
    s_ms.cfg.vmax = 8000U;
    s_ms.cfg.t_acc_ms = 10U;
    s_ms.cfg.wheel_rate = 12U;
}

void keymouse_reset(void)
{
    memset(s_ms.btn_cnt, 0, sizeof(s_ms.btn_cnt));
    memset(s_ms.ch, 0, sizeof(s_ms.ch));
    memset(s_ms.axis, 0, sizeof(s_ms.axis));
    s_ms.frame_cnt = 0U;
    s_ms.immediate = 0U;
    s_ms.pending = 0U;
    s_ms.fail_cnt = 0U;
}

void keymouse_press(uint8_t ki, const rotkc_t *kc)
{
    uint8_t sy = (uint8_t)kc->ms.sy;
    uint8_t mask;
    int8_t idx;

    if (sy == MS_BTN) {
        mask = (uint8_t)kc->ms.co;
        if (mask == 0U) {
            hal_dt_printf(TAG "press invalid btn=0 ki=%u\r\n", (unsigned)ki);
            return;
        }
        if (_btn_apply(mask, true)) {
            s_ms.immediate = 1U;
        }
        return;
    }

    idx = _ch_index(sy);
    if (idx >= 0) {
        if (s_ms.ch[idx].cnt == 0U) {
            s_ms.ch[idx].start_frame = s_ms.tick;
            s_ms.immediate = 1U;
        }
        if (s_ms.ch[idx].cnt < 0xFFU) {
            s_ms.ch[idx].cnt++;
        }
        return;
    }

    hal_dt_printf(TAG "press invalid sy=%u ki=%u\r\n", (unsigned)sy, (unsigned)ki);
}

void keymouse_release(uint8_t ki, const rotkc_t *kc)
{
    uint8_t sy = (uint8_t)kc->ms.sy;
    uint8_t mask;
    int8_t idx;

    if (sy == MS_BTN) {
        mask = (uint8_t)kc->ms.co;
        if (mask == 0U) {
            hal_dt_printf(TAG "release invalid btn=0 ki=%u\r\n", (unsigned)ki);
            return;
        }
        if (_btn_apply(mask, false)) {
            s_ms.immediate = 1U;
        }
        return;
    }

    idx = _ch_index(sy);
    if (idx >= 0) {
        if (s_ms.ch[idx].cnt == 0U) {
            hal_dt_printf(TAG "release underflow ch=%d ki=%u\r\n", (int)idx, (unsigned)ki);
            return;
        }
        s_ms.ch[idx].cnt--;
        if (s_ms.ch[idx].cnt == 0U) {
            _axis_clear_if_idle((uint8_t)(idx >> 1));
            s_ms.immediate = 1U;
        }
        return;
    }

    hal_dt_printf(TAG "release invalid sy=%u ki=%u\r\n", (unsigned)sy, (unsigned)ki);
}

bool keymouse_process(void)
{
    bool due = false;
    bool had_immediate = false;
    bool active;
    int16_t dx;
    int16_t dy;
    int16_t dw;

    s_ms.tick++;
    s_ms.frame_cnt++;

    if (s_ms.immediate != 0U) {
        due = true;
        had_immediate = true;
    }
    else if (s_ms.pending != 0U) {
        due = true;
    }
    else if (s_ms.frame_cnt >= MS_RPT_PERIOD_FRAMES) {
        due = true;
    }

    if (!due) {
        return false;
    }

    if (had_immediate) {
        s_ms.immediate = 0U;
        s_ms.frame_cnt = 0U;
    }
    else if (s_ms.frame_cnt >= MS_RPT_PERIOD_FRAMES) {
        s_ms.frame_cnt -= MS_RPT_PERIOD_FRAMES;
    }

    active = _any_active();
    if (!active && !had_immediate && (s_ms.pending == 0U)) {
        return false; /* 空闲静默，不产生全零报告流 */
    }

    dx = _axis_output(0U);
    dy = _axis_output(1U);
    dw = _wheel_output();

    /* 只有按键沿（immediate）、重发（pending）或产生非零位移时才上报；
     * 按住按键但不动时不发重复报告，报告内容无变化就没有发送必要。 */
    if (!had_immediate && (s_ms.pending == 0U) &&
        (dx == 0) && (dy == 0) && (dw == 0)) {
        return false;
    }

    keyrpt_ms_set_move(dx, dy);
    keyrpt_ms_set_wheel(dw);
    return true;
}

bool keymouse_active(void)
{
    return _any_active() || (s_ms.immediate != 0U) || (s_ms.pending != 0U);
}

void keymouse_tx_result(int result)
{
    if ((result == 0) || (result == -2)) {
        /* 成功，或 USB 未配置（状态照常演进，由世代机制接管清零） */
        s_ms.pending = 0U;
        s_ms.fail_cnt = 0U;
        return;
    }

    s_ms.pending = 1U;
    if (s_ms.fail_cnt < 0xFFU) {
        s_ms.fail_cnt++;
    }
    if (s_ms.fail_cnt >= 3U) {
        hal_dt_printf(TAG "send failed: %d\r\n", result);
        s_ms.fail_cnt = 0U;
    }
}

const keymouse_cfg_t *keymouse_cfg_get(void)
{
    return &s_ms.cfg;
}

rotsta_t keymouse_cfg_set(const keymouse_cfg_t *cfg)
{
    if (cfg == NULL) {
        return rotsta_invalid_argument;
    }
    if ((cfg->v0 == 0U) || (cfg->v0 > cfg->vmax)) {
        return rotsta_invalid_argument;
    }
    if ((cfg->vmax == 0U) || (cfg->vmax > 15000U)) {
        return rotsta_invalid_argument;
    }
    if ((cfg->t_acc_ms < 50U) || (cfg->t_acc_ms > 5000U)) {
        return rotsta_invalid_argument;
    }
    if ((cfg->wheel_rate == 0U) || (cfg->wheel_rate > 100U)) {
        return rotsta_invalid_argument;
    }

    s_ms.cfg = *cfg;
    return rotsta_success;
}