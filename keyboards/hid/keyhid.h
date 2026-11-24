#ifndef KEY_HID_H
#define KEY_HID_H

#include <stdint.h>

/* 键盘打字：把按键触发事件翻译成 HID 键码并通过 USB 键盘接口上报。
 * 必须在 keyevt/keymap/keyprop 初始化之后调用。 */
void keyhid_init(void);

/* 每帧扫描完成后调用一次，处理本帧的按下/抬起沿。 */
void keyhid_process(void);

/* 释放所有按键并清空内部状态（校准等禁止打字的场景使用）。 */
void keyhid_suspend(void);

#endif  /* KEY_HID_H */
