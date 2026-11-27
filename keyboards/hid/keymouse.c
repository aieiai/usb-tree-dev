/**
 * @Brief:  mouse key state machine: button/motion hold counts, speed model and report cadence.
 */
#include "keymouse.h"
#include "keyrpt.h"

#include "usb/diagnostic_tracking/hal_dt.h"

#include <string.h>

#define TAG "keymouse: "

#define MS_FRAMES_PER_MS        (8U)    /* 扫描帧率 8kHz */
#define MS_RPT_PERIOD_FRAMES    (MS_RPT_PERIOD_MS * MS_FRAMES_PER_MS)
#define MS_AXIS_MAX_OUTPUT      (120)
#define MS_AXIS_Q16_ONE         (65536U)

#define MS_BTN_NUM              (5U)
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

/* 按钮功能下标 -> 报告按钮位（bit0..4 左/右/中/后退/前进） */
static const uint8_t s_btn_bit[MS_BTN_NUM] = {
    MS_BTN1, MS_BTN2, MS_BTN3, MS_BTN4, MS_BTN5
};

static int8_t _btn_index(uint8_t sy)
{
    switch (sy) {
    case MS_BL: return 0;
    case MS_BR: return 1;
    case MS_BM: return 2;
    case MS_BB: return 3;
    case MS_BP: return 4;
    default:    return -1;
    }
}

/* 移动/滚轮通道下标：X+/X-/Y+/Y-/W+/W-，同轴正反为相邻通道 */
static int8_t _ch_index(uint8_t sy)
{
    switch (sy) {
    case MS_XP: return 0;
    case MS_XN: return 1;
    case MS_YP: return 2;
    case MS_YN: return 3;
    case MS_WP: return 4;
    case MS_WN: return 5;
    default:    return -1;
    }
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
                       MS_RPT_PERIOD_MS * MS_AXIS_Q16_ONE) / 1000U);

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

    dq16 = (uint32_t)(((uint32_t)s_ms.cfg.wheel_rate * MS_RPT_PERIOD_MS * MS_AXIS_Q16_ONE) / 1000U);

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
    s_ms.cfg.v0 = 800U;
    s_ms.cfg.vmax = 4000U;
    s_ms.cfg.t_acc_ms = 500U;
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
    int8_t idx;

    idx = _btn_index(sy);
    if (idx >= 0) {
        if (s_ms.btn_cnt[idx] == 0U) {
            keyrpt_ms_add_btn(s_btn_bit[idx]);
            s_ms.immediate = 1U;
        }
        if (s_ms.btn_cnt[idx] < 0xFFU) {
            s_ms.btn_cnt[idx]++;
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
    int8_t idx;

    idx = _btn_index(sy);
    if (idx >= 0) {
        if (s_ms.btn_cnt[idx] == 0U) {
            hal_dt_printf(TAG "release underflow btn=%d ki=%u\r\n", (int)idx, (unsigned)ki);
            return;
        }
        s_ms.btn_cnt[idx]--;
        if (s_ms.btn_cnt[idx] == 0U) {
            keyrpt_ms_del_btn(s_btn_bit[idx]);
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