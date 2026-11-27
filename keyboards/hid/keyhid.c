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

typedef struct {
    keyhid_key_t kbd[KBD_KEY_MAX];
    bool dirty_kbd;
    bool dirty_csm;
    bool dirty_sys;
    uint32_t usb_gen;
} keyhid_t;

static keyhid_t keyhid;

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
        if ((ki != except) && (keyhid.kbd[ki].out == code)) {
            return true;
        }
    }
    return false;
}

static inline uint8_t _active_layer(void)
{
    keyhid_t *p_hid = &keyhid;
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        if (p_hid->kbd[ki].fn_layer != KEYHID_NO_FN) {
            return p_hid->kbd[ki].fn_layer;
        }
    }
    return keymap_get_active_layer();
}

static inline void _report_send_id(uint8_t id)
{
    const uint8_t *buf = NULL;
    uint16_t len = keyrpt_get_raw(id, &buf);

    if ((buf != NULL) && (len != 0U)) {
        (void)hal_kbd_send(buf, len, KEYHID_TX_TIMEOUT_MS);
    }
}

static inline void _report_send(void)
{
    _report_send_id((g_obi.fs.nkro != 0U) ? KRPT_ID_NKRO : KRPT_ID_6KRO);
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
    keyhid_t *p_hid = &keyhid;
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        rotkc_t kc = { .code = p_hid->kbd[ki].out };

        if ((p_hid->kbd[ki].out != 0U) && (kc.ty == KCT_MS)) {
            p_hid->kbd[ki].out = 0U;
        }
    }
    keymouse_reset();
    keyrpt_ms_clr();
}

/* Fn 键在按下时记录层号，不产生 HID 上报。
 * 其余 FN_* 子类型（灯效/蓝牙/2.4G/显示/快捷键）暂无固件后端。 */
static inline void _fn_press(uint8_t ki, const rotkc_t *kc)
{
    /* _FN()/_FL()... 把功能类型写在 sy 位域，co 保存层号或功能号 */
    if (kc->fn.sy == FN_KEY) {
        keyhid.kbd[ki].fn_layer = _fn_layer((uint16_t)kc->fn.co);
    }
}

/* 多媒体/系统键松开：当前上报值正好是它时，改成仍按住的同类键。 */
static inline void _csm_release(uint16_t cc)
{
    keyrpt_csm_del(cc);
    keyhid_t *p_hid = &keyhid;
    for (uint8_t i = 0U; i < KBD_KEY_MAX; i++) {
        rotkc_t held = { .code = p_hid->kbd[i].out };

        if (held.ty == KCT_CSM) {
            keyrpt_csm_add(held.csm.co);
            break;
        }
    }
    p_hid->dirty_csm = true;
}

static inline void _sys_release(uint8_t sc)
{
    keyrpt_sys_del(sc);
    keyhid_t *p_hid = &keyhid;
    for (uint8_t i = 0U; i < KBD_KEY_MAX; i++) {
        rotkc_t held = { .code = p_hid->kbd[i].out };

        if (held.ty == KCT_SYS) {
            keyrpt_sys_add((uint8_t)held.sys.co);
            break;
        }
    }
    p_hid->dirty_sys = true;
}

static inline void _key_press(uint8_t ki, uint8_t layer)
{
    rotkc_t kc;
    keyhid_t *p_hid = &keyhid;

    p_hid->kbd[ki].out = 0U;

    /* 已识别为 Fn 的按键不产生 HID 上报 */
    if (p_hid->kbd[ki].fn_layer != KEYHID_NO_FN) {
        return;
    }

    kc.code = keymap_get_key(layer, ki);

    /* Fn 键只切换层，不产生 HID 上报。 */
    if (kc.ty == KCT_FN) {
        _fn_press(ki, &kc);
        return;
    }

    /* 鼠标键码：记录按下键码后交给 keymouse 计数，不进入键盘打字路径。 */
    if (kc.ty == KCT_MS) {
        p_hid->kbd[ki].out = kc.code;
        keymouse_press(ki, &kc);
        return;
    }

    /* 多媒体键：报告只装一个 usage，后按下的覆盖先按下的。 */
    if (kc.ty == KCT_CSM) {
        p_hid->kbd[ki].out = kc.code;
        keyrpt_csm_add(kc.csm.co);
        p_hid->dirty_csm = true;
        return;
    }

    /* 系统键：关机/睡眠/唤醒/菜单类 usage。 */
    if (kc.ty == KCT_SYS) {
        p_hid->kbd[ki].out = kc.code;
        keyrpt_sys_add((uint8_t)kc.sys.co);
        p_hid->dirty_sys = true;
        return;
    }

    /* 其余键码（高级/手柄等）暂不参与上报。 */
    if (kc.ty != KCT_KBD) {
        return;
    }
    if ((kc.kbd.co == KC_NONE) || (kc.kbd.co == KC_UNDEFINED)) {
        return;
    }

    p_hid->kbd[ki].out = kc.code;
    if (_code_held(ki, kc.code)) {
        return;
    }

    keyrpt_6kro_add(kc.kbd.co);
    keyrpt_nkro_add(kc.kbd.co);
    p_hid->dirty_kbd = true;
}

static inline void _key_release(uint8_t ki)
{
    keyhid_t *p_hid = &keyhid;
    uint32_t code = p_hid->kbd[ki].out;
    rotkc_t kc = { .code = code };

    p_hid->kbd[ki].out = 0U;
    p_hid->kbd[ki].fn_layer = KEYHID_NO_FN;

    if (code == 0U) {
        return;
    }

    /* 鼠标键按按下时的键码对称递减持有计数，不依赖 _code_held。 */
    if (kc.ty == KCT_MS) {
        keymouse_release(ki, &kc);
        return;
    }

    if (kc.ty == KCT_CSM) {
        _csm_release(kc.csm.co);
        return;
    }

    if (kc.ty == KCT_SYS) {
        _sys_release((uint8_t)kc.sys.co);
        return;
    }

    if ((kc.ty != KCT_KBD) || _code_held(ki, code)) {
        return;
    }

    keyrpt_6kro_del(kc.kbd.co);
    keyrpt_nkro_del(kc.kbd.co);
    p_hid->dirty_kbd = true;
}

void keyhid_init(void)
{
    keyhid_t *p_hid = &keyhid;
    memset(p_hid->kbd, 0, sizeof(p_hid->kbd));
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        p_hid->kbd[ki].fn_layer = KEYHID_NO_FN;
    }
    p_hid->dirty_kbd = false;
    p_hid->dirty_csm = false;
    p_hid->dirty_sys = false;

    keymouse_init();
    p_hid->usb_gen = hal_kbd_usb_generation();
}

void keyhid_process(void)
{
    uint8_t layer;
    uint32_t usb_gen;
    keyhid_t *p_hid = &keyhid;

    usb_gen = hal_kbd_usb_generation();
    if (usb_gen != p_hid->usb_gen) {
        p_hid->usb_gen = usb_gen;
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
    if (p_hid->dirty_kbd) {
        p_hid->dirty_kbd = false;
        _report_send();
    }

    if (p_hid->dirty_csm) {
        p_hid->dirty_csm = false;
        _report_send_id(KRPT_ID_CSM);
    }

    if (p_hid->dirty_sys) {
        p_hid->dirty_sys = false;
        _report_send_id(KRPT_ID_SYS);
    }

    if (keymouse_process()) {
        _ms_report_send();
    }
}

void keyhid_suspend(void)
{
    bool active = keymouse_active();
    keyhid_t *p_hid = &keyhid;

    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        _key_release(ki);
    }

    keymouse_reset();
    keyrpt_ms_clr();

    if (p_hid->dirty_kbd) {
        p_hid->dirty_kbd = false;
        _report_send();
    }

    if (p_hid->dirty_csm) {
        p_hid->dirty_csm = false;
        _report_send_id(KRPT_ID_CSM);
    }

    if (p_hid->dirty_sys) {
        p_hid->dirty_sys = false;
        _report_send_id(KRPT_ID_SYS);
    }

    /* 校准挂起期间本函数每帧被调用，仅首次（存在鼠标状态时）补发全零报告。 */
    if (active) {
        _ms_zero_report_send();
    }
}
