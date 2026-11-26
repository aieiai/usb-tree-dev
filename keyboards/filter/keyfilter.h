/**
 * @Brief:  key adc dynamic filter.
 * @Date:   2026/11/26
 *
 *          按键 ADC 原始值的动态滤波。
 *
 *          设计目标：
 *            1. 静止时输出稳定，不随传感器噪声抖动（噪声衰减 14 倍以上）；
 *            2. 位置变化超过门限时平滑系数立刻抬高，近似直通，实时跟随；
 *            3. 单拍尖峰（EMI、ADC 误码）不进入后续触发逻辑；
 *            4. 全整数定点运算，8kHz x 68 键不占浮点预算（本工程 ABI 为 ilp32）。
 *
 *          算法：三点中值 -> 残差自适应 EMA -> 物理斜率限幅。
 *          每键每拍约 110 条整数指令（约 0.29us @400MHz），无除法、无浮点。
 */
#ifndef KEY_FILTER_H
#define KEY_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#include "kbd_config.h"

/*
 * 可调参数（单位 ADC counts；默认标定下满行程约 32764 counts / 4mm，
 * 即 1 count ≈ 0.12um，8191 counts ≈ 1mm）
 *
 * KFILT_E0    静止门限。残差小于该值时认为按键静止，使用最重的平滑。
 *             取实测噪声（keyfilter_get_noise()）的 3~4 倍。默认 40 counts
 *             可覆盖原始噪声 sigma <= 30 的传感器（衰减 13 倍以上）。
 * KFILT_E1    跟随门限。残差大于该值时平滑系数升到 KFILT_AMAX。
 * KFILT_A0    静止平滑系数 1/128：噪声衰减约 15 倍，时间常数 128 拍（16ms @8kHz）。
 * KFILT_AMAX  跟随平滑系数 0.8：残差超过 E1 后每拍消掉 80%，高速段滞后约
 *             (1-a)/a = 0.25 拍位移（800 counts/拍 时约 0.024mm，可忽略）。
 *             不要取 1.0：完全直通会把残差清零，下一拍又回到重平滑，形成
 *             周期性台阶（幅度约 E1），匀速段位置会看到锯齿。
 *             只要 a < 1，a(e)*e 就单调递增，固定点唯一，任何速度下都不会自激。
 * KFILT_SLEW  单拍最大变化量。仅用于拦截连续多拍尖峰，正常击键达不到该速度
 *             （1536 counts/拍 ≈ 1.5m/s ≈ 0.19mm/拍，人手极限约 1m/s）。
 *
 * 默认参数实测（8kHz，主机仿真）：
 *   静止噪声 20 counts -> 输出 1.4 counts（衰减 14 倍，0.17um）
 *   0.78m/s 快速下压     -> 最大跟随误差 0.024mm，停止后 1 拍收敛到 0.01mm 内
 *   0.2m/s 中速          -> 滞后 0.006mm
 *   单拍 +8000 尖峰      -> 输出 0 偏移（完全剔除）
 *
 * 现场调参：
 *   静止抖动仍可见   -> 调大 KFILT_E0（先看实测噪声），或调小 KFILT_A0；
 *   快速击键跟不上   -> 调大 KFILT_AMAX，或调小 KFILT_E1。
 */
#ifndef KFILT_E0
#   define KFILT_E0         (40)
#endif

#ifndef KFILT_E1
#   define KFILT_E1         (200)
#endif

#ifndef KFILT_A0
#   define KFILT_A0         (1.0f / 128.0f)
#endif

#ifndef KFILT_AMAX
#   define KFILT_AMAX       (0.8f)
#endif

#ifndef KFILT_SLEW
#   define KFILT_SLEW       (1536)
#endif

#if (KFILT_E1 <= KFILT_E0)
#   error "KFILT_E1 must be greater than KFILT_E0"
#endif

#if (KFILT_E1 > 65535) || (KFILT_E0 < 0) || (KFILT_SLEW <= 0)
#   error "invalid key filter threshold"
#endif

/* 自适应系数表按 |e| 展开，E1 决定表长（4 字节/项），不要调得过大 */
#if (KFILT_E1 > 2048)
#   error "KFILT_E1 too large: alpha lookup table would exceed 8KB"
#endif

void     keyfilter_init(void);
uint16_t keyfilter_update(uint8_t ki, uint16_t raw);
uint16_t keyfilter_get(uint8_t ki);
uint16_t keyfilter_get_noise(uint8_t ki);
void     keyfilter_reset(uint8_t ki);
void     keyfilter_reset_all(void);

#endif  /* KEY_FILTER_H */
