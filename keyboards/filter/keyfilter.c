/**
 * @Brief:  key adc dynamic filter.
 * @Date:   2026/11/26
 */
#include "keyfilter.h"

#include <string.h>

/* 内部状态保留 8 位小数：小系数下 (a * e) 不会被截断成 0，避免出现死区 */
#define KFILT_Q         (8)
#define KFILT_Q_HALF    (1L << (KFILT_Q - 1))
#define KFILT_Q16_ONE   (1L << 16)

typedef struct {
    uint16_t h1;     /* 上一拍原始值 */
    uint16_t h2;     /* 上上拍原始值 */
    uint32_t noise;  /* 残差绝对值均值，Q8，仅供观测调参 */
    int32_t  y;      /* 滤波输出，Q8 */
    uint8_t  primed; /* 首拍直接锁存，避免上电从 0 缓慢爬升 */
} kfilt_t;

static kfilt_t s_kfilt[KBD_KEY_MAX];

/* 参数在 init 时由浮点宏折算成整数，运行期只做整数运算。
 * 打包成一个结构体，热路径只取一次基址，省掉 6 次 lui/addi 地址计算。 */
typedef struct {
    uint32_t a0;       /* 静止平滑系数，Q16 */
    uint32_t amax;     /* 跟随平滑系数，Q16 */
    uint32_t e0;       /* 静止门限，counts */
    uint32_t e1;       /* 跟随门限，counts */
    uint32_t span;     /* E1 - E0 */
    uint32_t e_scale;  /* 256 * 65536 / span */
    int32_t  slew;     /* 单拍最大变化量，Q8 */
    uint16_t slew_cnt; /* 单拍最大变化量，counts */
    uint16_t gate;     /* 三拍累计变化超过该值才认为是真实运动，counts */
    /* 自适应系数表：a = f(|e|)，init 时按二次软拐点展开，热路径只查表 */
    uint32_t atab[KFILT_E1 + 1];
} kfilt_cfg_t;

static kfilt_cfg_t s_cfg;

static inline uint16_t _median3(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) {
        uint16_t t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        b = c;
    }
    return (a > b) ? a : b;
}

/**
 * @brief:  Reject single sample spikes.
 *
 *          连续同向、每拍步长在物理极限内、且三拍累计变化明显高于噪声时，
 *          说明是真实运动，直接取本拍，不引入任何延迟；
 *          否则取三点中值，把单拍尖峰和噪声折返挡在外面。
 *          尖峰即使恰好延续前两拍的趋势，也会因为步长超限而走中值分支。
 */
static inline uint16_t _spike_reject(uint16_t x0, uint16_t x1, uint16_t x2, uint16_t lim, uint16_t gate)
{
    uint16_t d1 = (x0 > x1) ? (uint16_t)(x0 - x1) : (uint16_t)(x1 - x0);
    uint16_t d2 = (x1 > x2) ? (uint16_t)(x1 - x2) : (uint16_t)(x2 - x1);

    if ((d1 <= lim) && (d2 <= lim) && ((uint32_t)d1 + d2 >= gate)) {
        if (((x2 <= x1) && (x1 <= x0)) || ((x2 >= x1) && (x1 >= x0))) {
            return x0;
        }
    }
    return _median3(x0, x1, x2);
}

static inline uint16_t _q8_to_cnt(int32_t y)
{
    int32_t cnt = (y + KFILT_Q_HALF) >> KFILT_Q;

    if (cnt < 0) {
        cnt = 0;
    }
    else if (cnt > 0xFFFF) {
        cnt = 0xFFFF;
    }
    return (uint16_t)cnt;
}

/**
 * @brief:  Filter one key sample.
 * @param:  The index of the key.
 * @param:  The raw adc value of this frame.
 * @return: The filtered value.
 *
 *          1. 三点中值剔除单拍尖峰（连续同向且步长合理时零延迟放行）；
 *          2. 残差 |e| 决定平滑系数 a：
 *             |e| <= E0      -> a = A0，静止重平滑；
 *             E0 < |e| < E1  -> 二次软拐点，a 从 A0 平滑升到 AMAX；
 *             |e| >= E1      -> a = AMAX，每拍消掉大部分残差，实时跟随；
 *             二次曲线在拐点处一阶连续，门限附近不会来回切换产生台阶；
 *          3. 单拍变化量按物理极限限幅，兜住连续多拍尖峰。
 */
uint16_t keyfilter_update(uint8_t ki, uint16_t raw)
{
    kfilt_t *p = &s_kfilt[ki];

    if (p->primed == 0U) {
        p->primed = 1U;
        p->h1     = raw;
        p->h2     = raw;
        p->noise  = 0U;
        p->y      = (int32_t)raw << KFILT_Q;
        return raw;
    }

    const kfilt_cfg_t *c = &s_cfg;
    uint16_t x = _spike_reject(raw, p->h1, p->h2, c->slew_cnt, c->gate);
    p->h2 = p->h1;
    p->h1 = raw;

    /* 残差用 counts 计算并限幅到 +-2mm：超过该量级平滑系数已经饱和，
     * 限幅后 32 位乘法不会溢出，省掉一次 64 位乘。 */
    int32_t ec = (int32_t)x - (int32_t)((p->y + KFILT_Q_HALF) >> KFILT_Q);

    if (ec > 16384) {
        ec = 16384;
    }
    else if (ec < -16384) {
        ec = -16384;
    }

    uint32_t ae = (uint32_t)((ec < 0) ? -ec : ec);

    /* 查表取平滑系数：|e| 超过 E1 后统一用最大系数，表尾即饱和值 */
    uint32_t a = c->atab[(ae < c->e1) ? ae : c->e1];

    int32_t d = ((int32_t)a * ec) >> 8; /* Q16 * counts -> Q8 */

    if (d > c->slew) {
        d = c->slew;
    }
    else if (d < -c->slew) {
        d = -c->slew;
    }

    p->y += d;

    if (p->y < 0) {
        p->y = 0;
    }
    else if (p->y > ((int32_t)0xFFFF << KFILT_Q)) {
        p->y = (int32_t)0xFFFF << KFILT_Q;
    }

    /* 残差均值，约等于 0.8 倍噪声标准差；静止时读取可用来标定 KFILT_E0。
     * 用 Q8 累加，否则小残差右移后会截断成 0，估计值永远为 0。 */
    int32_t dn = ((int32_t)(ae << 8) - (int32_t)p->noise) >> 5;
    p->noise = (uint32_t)((int32_t)p->noise + dn);

    return _q8_to_cnt(p->y);
}

uint16_t keyfilter_get(uint8_t ki)
{
    if (ki >= KBD_KEY_MAX) {
        return 0U;
    }
    return _q8_to_cnt(s_kfilt[ki].y);
}

/**
 * @brief:  Get the residual mean of one key, read it while the key is at rest.
 * @return: Mean of |e| in adc counts, about 0.8 x noise sigma.
 */
uint16_t keyfilter_get_noise(uint8_t ki)
{
    if (ki >= KBD_KEY_MAX) {
        return 0U;
    }
    return (uint16_t)(s_kfilt[ki].noise >> 8);
}

void keyfilter_reset(uint8_t ki)
{
    if (ki >= KBD_KEY_MAX) {
        return;
    }
    memset(&s_kfilt[ki], 0, sizeof(s_kfilt[ki]));
}

void keyfilter_reset_all(void)
{
    memset(s_kfilt, 0, sizeof(s_kfilt));
}

void keyfilter_init(void)
{
    kfilt_cfg_t *c = &s_cfg;

    c->a0     = (uint32_t)(KFILT_A0 * 65536.0f + 0.5f);
    c->amax   = (uint32_t)(KFILT_AMAX * 65536.0f + 0.5f);
    c->e0     = (uint32_t)KFILT_E0;
    c->e1     = (uint32_t)KFILT_E1;
    c->span   = (uint32_t)(KFILT_E1 - KFILT_E0);
    c->e_scale = (uint32_t)((256.0f * 65536.0f) / (float)c->span + 0.5f);
    c->slew   = (int32_t)((uint32_t)KFILT_SLEW << KFILT_Q);

    /* 防止宏被改成非法值后出现除零、反向平滑或 16 位截断 */
    if (c->a0 == 0U) {
        c->a0 = 1U;
    }
    if (c->amax > KFILT_Q16_ONE) {
        c->amax = KFILT_Q16_ONE;
    }
    if (c->amax < c->a0) {
        c->amax = c->a0;
    }
    if (c->span == 0U) {
        c->span = 1U;
    }
    if (c->slew <= 0) {
        c->slew = 1;
    }

    c->slew_cnt = (KFILT_SLEW > 0xFFFF) ? 0xFFFFU : (uint16_t)KFILT_SLEW;
    c->gate     = (c->e0 > 0x3FFFU) ? 0xFFFFU : (uint16_t)(4U * c->e0);

    /* 展开自适应系数表：二次软拐点，拐点处一阶连续，门限附近不会切换出台阶 */
    for (uint32_t i = 0U; i <= c->e1; i++) {
        uint32_t a = c->a0;

        if (i > c->e0) {
            uint32_t de = i - c->e0;

            if (de >= c->span) {
                a = c->amax;
            }
            else {
                uint32_t t = (de * c->e_scale) >> 16; /* 0..255 */
                a = c->a0 + (((((c->amax - c->a0) * t) >> 8) * t) >> 8);
            }
        }
        c->atab[i] = a;
    }

    keyfilter_reset_all();
}
