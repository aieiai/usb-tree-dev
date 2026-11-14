#ifndef ROTKC_H
#define ROTKC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// key code type
enum { KCT_KBD = 0x00, KCT_SYS, KCT_CSM, KCT_ADV, KCT_MS, KCT_GP, KCT_FN, KCT_MAX };
// function customization type
enum { FN_UT = 0x00, FN_LIGHT, FN_BT, FN_RADIO, FN_DISP, FN_SHCT, FN_MAX };



// key code
typedef union {
    uint32_t code;
    struct __attribute__((packed)) {
        uint32_t lk: 1; // Lock
        uint32_t ty: 6; // Easy to search/query
        uint32_t rv: 25;
    };
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 18; uint32_t co: 8;  } kbd; // Keyboard
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 18; uint32_t co: 8;  } sys; // System
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 10; uint32_t co: 16; } csm; // Consumer
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 13; uint32_t co: 8;  } adv; // Advanced
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 5;  uint32_t co: 16; } ms;  // Mouse
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 5;  uint32_t co: 16; } gp;  // Gamepad
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t fy: 5;  uint32_t co: 16; } fn;  // Function customization
} __attribute__((aligned(4))) rotkc_t;
_Static_assert(4 == sizeof(rotkc_t), "rotkc_t must be 4 bytes");

#define _KB(co)     { .lk = 0, .ty = KCT_KBD, .kbd = { .rv = 0, .co = co } } /* Keyboard */
#define _KS(co)     { .lk = 0, .ty = KCT_SYS, .sys = { .rv = 0, .co = co } } /* System */
#define _KC(co)     { .lk = 0, .ty = KCT_CSM, .csm = { .rv = 0, .co = co } } /* Consumer */
#define _KA(co)     { .lk = 0, .ty = KCT_ADV, .adv = { .sy = 0, .rv = 0, .co = co } } /* Advanced */
#define _KM(co)     { .lk = 0, .ty = KCT_MS,  .ms  = { .sy = 0, .rv = 0, .co = co } } /* Mouse */
#define _KG(co)     { .lk = 0, .ty = KCT_GP,  .gp  = { .rv = 0, .rv = 0, .co = co } } /* Gamepad */
#define _LK(ty, co) { .lk = 1, .ty = ty,      .kbd = { .rv = 0, .co = co } } /* key lock */

#define ___(co)     { .lk = 0, .ty = KCT_FN,  .fn  = { .sy = FN_UT,    .fy = 0, .co = co } } /* fn Unvarnished transmission */
#define _FL(co)     { .lk = 0, .ty = KCT_FN,  .fn  = { .sy = FN_LIGHT, .fy = 0, .co = co } } /* fn Lights */
#define _FB(co)     { .lk = 0, .ty = KCT_FN,  .fn  = { .sy = FN_BT,    .fy = 0, .co = co } } /* fn Bluetooth */
#define _FR(co)     { .lk = 0, .ty = KCT_FN,  .fn  = { .sy = FN_RADIO, .fy = 0, .co = co } } /* fn Radio */
#define _FD(co)     { .lk = 0, .ty = KCT_FN,  .fn  = { .sy = FN_DISP,  .fy = 0, .co = co } } /* fn Display */
#define _FS(co)     { .lk = 0, .ty = KCT_FN,  .fn  = { .sy = FN_SHCT,  .fy = 0, .co = co } } /* fn Shortcut */



#ifdef __cplusplus
}
#endif

#endif
