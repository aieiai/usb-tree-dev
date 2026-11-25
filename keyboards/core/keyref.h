#ifndef KEY_REF_H
#define KEY_REF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 键码引用计数。
 * code 为最终要输出的 8 位键码（当前是 HID keyboard usage，含 0xE0..0xE7 修饰键）。
 * 多个来源（物理键、Fn 层、宏/高级键等）可以同时持有一个键码，只有最后一个
 * 持有者释放时才真正抬起。上报层通过 keyref_get_edge() 感知其他模块造成的变化。 */
bool keyref_clr(void);

/* 返回 true 表示引用计数 0 -> 1。 */
bool keyref_inc(uint8_t code);

/* 返回 true 表示引用计数 1 -> 0。 */
bool keyref_dec(uint8_t code);

/* 当前活动键码（引用计数 > 0），顺序不保证，供上报层重建完整报告。 */
uint8_t keyref_get_cnt(void);
const uint8_t *keyref_get_list(void);

/* 每次 0 <-> 1 边沿跳变递增；上报层用它发现非本模块产生的键码变化。 */
uint32_t keyref_get_edge(void);

#ifdef __cplusplus
}
#endif

#endif  /* KEY_REF_H */
