/**
 * @Brief:  keyboard typing. Map key events to HID key codes and report them.
 */
#include "keyhid.h"

#include "kbd_config.h"
#include "keycode.h"
#include "keyevt.h"
#include "keymap.h"
#include "keymouse.h"
#include "keyprop.h"
#include "keyrpt.h"

#include "usb/keyboard/hal_kbd.h"

#include <string.h>

/* 等待 IN 传输完成的时间：主机按端点间隔轮询，正常情况下远小于该值。 */
#define KEYHID_TX_TIMEOUT_MS    (5U)
#define KEYHID_NO_FN            (0xFFU)

typedef struct {
    uint32_t out;       /* 按下期间上报的键码，0 表示不上报（Fn 等） */
    uint8_t  fn_layer;  /* FN_KEY 对应的层，KEYHID_NO_FN 表示不是 Fn 键 */
} keyhid_key_t;

static keyhid_key_t s_kbd[KBD_KEY_MAX];
static bool s_dirty_kbd;
static uint32_t s_usb_gen;

/* KC_FN1..KC_FN7（0xF1..0xF7）与上位机下发的 1..7 都表示层号。 */
static inline uint8_t _fn_layer(uint16_t co)
{
    uint8_t layer = (co >= KC_FN0) ? (uint8_t)(co - KC_FN0) : (uint8_t)co;

    if (layer == 0U) {
        layer = 1U;
    }
    return (layer < KBD_MAP_MAX) ? layer : (uint8_t)(KBD_MAP_MAX - 1U);
}

/* 同一键码是否还被其他物理按键持有（例如 Fn 层映射重叠）。 */
static inline bool _code_held(uint8_t except, uint32_t code)
{
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        if ((ki != except) && (s_kbd[ki].out == code)) {
            return true;
        }
    }
    return false;
}

static inline uint8_t _active_layer(void)
{
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        if (s_kbd[ki].fn_layer != KEYHID_NO_FN) {
            return s_kbd[ki].fn_layer;
        }
    }
    return keymap_get_active_layer();
}

static inline void _report_send(void)
{
    const uint8_t *buf = NULL;
    uint8_t id = (g_obi.fs.nkro != 0U) ? KRPT_ID_NKRO : KRPT_ID_6KRO;
    uint16_t len = keyrpt_get_raw(id, &buf);

    if ((buf != NULL) && (len != 0U)) {
        (void)hal_kbd_send(buf, len, KEYHID_TX_TIMEOUT_MS);
    }
}

/* 鼠标报告经同一 IN 端点发送，失败结果交由 keymouse 决定重发。 */
static inline void _ms_report_send(void)
{
    const uint8_t *buf = NULL;
    uint16_t len = keyrpt_get_raw(KRPT_ID_MS, &buf);
    int result;

    if ((buf == NULL) || (len == 0U)) {
        return;
    }
    result = hal_kbd_send(buf, len, KEYHID_TX_TIMEOUT_MS);
    keymouse_tx_result(result);
}

/* 尽力发送一份全零鼠标报告，用于复位/挂起后清掉主机侧残留按键。 */
static inline void _ms_zero_report_send(void)
{
    const uint8_t *buf = NULL;
    uint16_t len = keyrpt_get_raw(KRPT_ID_MS, &buf);

    if ((buf != NULL) && (len != 0U)) {
        (void)hal_kbd_send(buf, len, KEYHID_TX_TIMEOUT_MS);
    }
}

/* USB 断开/复位/重配置后清空鼠标状态：同步清掉按住的鼠标键码，
 * 再做计数清零，避免随后的抬起沿对已清零计数递减造成下溢。 */
static inline void _ms_state_clear(void)
{
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        rotkc_t kc = { .code = s_kbd[ki].out };

        if ((s_kbd[ki].out != 0U) && (kc.ty == KCT_MS)) {
            s_kbd[ki].out = 0U;
        }
    }
    keymouse_reset();
    keyrpt_ms_clr();
}

/* Fn 键在按下时记录层号，不产生 HID 上报。 */
static inline void _fn_press(uint8_t ki, uint8_t layer)
{
    rotkc_t kc = { .code = keymap_get_key(layer, ki) };

    /* _FN()/_FL()... 把功能类型写在 sy 位域，co 保存层号或功能号 */
    if ((kc.ty == KCT_FN) && (kc.fn.sy == FN_KEY)) {
        s_kbd[ki].fn_layer = _fn_layer((uint16_t)kc.fn.co);
    }
}

static inline void _key_press(uint8_t ki, uint8_t layer)
{
    rotkc_t kc;

    s_kbd[ki].out = 0U;

    /* 已识别为 Fn 的按键不产生 HID 上报 */
    if (s_kbd[ki].fn_layer != KEYHID_NO_FN) {
        return;
    }

    kc.code = keymap_get_key(layer, ki);

    /* 鼠标键码：记录按下键码后交给 keymouse 计数，不进入键盘打字路径。 */
    if (kc.ty == KCT_MS) {
        s_kbd[ki].out = kc.code;
        keymouse_press(ki, &kc);
        return;
    }

    /* 普通键盘键码之外的键码不参与打字。 */
    if (kc.ty != KCT_KBD) {
        return;
    }
    if ((kc.kbd.co == KC_NONE) || (kc.kbd.co == KC_UNDEFINED)) {
        return;
    }

    s_kbd[ki].out = kc.code;
    if (_code_held(ki, kc.code)) {
        return;
    }

    keyrpt_6kro_add(kc.kbd.co);
    keyrpt_nkro_add(kc.kbd.co);
    s_dirty_kbd = true;
}

static inline void _key_release(uint8_t ki)
{
    uint32_t code = s_kbd[ki].out;
    rotkc_t kc = { .code = code };

    s_kbd[ki].out = 0U;
    s_kbd[ki].fn_layer = KEYHID_NO_FN;

    if (code == 0U) {
        return;
    }

    /* 鼠标键按按下时的键码对称递减持有计数，不依赖 _code_held。 */
    if (kc.ty == KCT_MS) {
        keymouse_release(ki, &kc);
        return;
    }

    if ((kc.ty != KCT_KBD) || _code_held(ki, code)) {
        return;
    }

    keyrpt_6kro_del(kc.kbd.co);
    keyrpt_nkro_del(kc.kbd.co);
    s_dirty_kbd = true;
}

void keyhid_init(void)
{
    memset(s_kbd, 0, sizeof(s_kbd));
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        s_kbd[ki].fn_layer = KEYHID_NO_FN;
    }
    s_dirty_kbd = false;

    keymouse_init();
    s_usb_gen = hal_kbd_usb_generation();
}

void keyhid_process(void)
{
    uint8_t layer;
    uint32_t usb_gen;

    usb_gen = hal_kbd_usb_generation();
    if (usb_gen != s_usb_gen) {
        s_usb_gen = usb_gen;
        _ms_state_clear();
        _ms_zero_report_send();
    }

    layer = _active_layer();

    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        if (KEVT_TRG_D2U == g_keyevt[ki].trg) {
            _key_release(ki);
        }
        else if (KEVT_TRG_U2D == g_keyevt[ki].trg) {
            _key_press(ki, layer);
        }
    }

    /* 键盘报告先行，保证打字延迟不受鼠标周期上报影响。 */
    if (s_dirty_kbd) {
        s_dirty_kbd = false;
        _report_send();
    }

    if (keymouse_process()) {
        _ms_report_send();
    }
}

void keyhid_suspend(void)
{
    bool ms_active = keymouse_active();

    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        _key_release(ki);
    }

    keymouse_reset();
    keyrpt_ms_clr();

    if (s_dirty_kbd) {
        s_dirty_kbd = false;
        _report_send();
    }

    /* 校准挂起期间本函数每帧被调用，仅首次（存在鼠标状态时）补发全零报告。 */
    if (ms_active) {
        _ms_zero_report_send();
    }
}