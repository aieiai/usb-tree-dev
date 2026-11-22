/**
 * @Author: wj
 * @Brief:  key attribute.
 * @Date:   2024/07/02
 */
#ifndef KEY_ATTR_H
#define KEY_ATTR_H

#include <stdint.h>
#include <stdbool.h>
#include "rotsta.h"

#include "kbd_config.h"
#include "keymagnet.h"
#include "keyaxis.h"
#include "keycode.h"


typedef enum {
    KTRG_TY_NOM = 0,
    KTRG_TY_RT,
    KTRG_TY_RT_CTN,
    KTRG_TY_ADV,
} ktrg_ty;


// on-board key property
typedef struct {
    uint32_t magic;
    uint32_t crc32;

    struct {
        kaxis_ty ty;
        kaxis_udef_ty udef_ty;
    } axis;

    // kop_type_t type_op;

    struct {
        uint16_t sclt;
        uint16_t sclto;
        uint16_t dcit;
        uint16_t dclt;
        uint16_t dclto;
    } tk; /* tick */

    // normal, rapid trigger, advanced
    ktrg_ty trg_ty;
    struct {
        uint16_t dst_p;      /* normal press dist */
        uint16_t dst_r;      /* normal raise dist */
    } nom;

    struct {
        uint16_t dst_t;     /* top dead zero dist */
        uint16_t dst_b;     /* bot dead zero dist */
    } dz;

    struct {
        uint8_t en : 1;
        uint8_t ctn_en : 1;
        uint8_t pls_en : 1;
        uint8_t rsv : 5;
        uint8_t pls_ty;
        uint16_t dst_p;     /* rapid trigger sensitivity press dist */
        uint16_t dst_r;     /* rapid trigger sensitivity raise dist */
        rotkc_t pls_kc[4];
    } rt;

    struct {
        // kadv_ty type;
        uint8_t tmp;
    } adv;
} __attribute__((aligned(4))) obkp_t;


// on-board information
typedef struct {
    uint32_t magic;
    uint32_t crc32;
    uint32_t version;
    uint8_t  name[32];

    // product information
    struct __attribute__((packed)) {
        uint64_t fv;     // Firmware Version
        uint64_t hv;     // Hardware Version
        uint16_t vid;    // Vendor ID
        uint16_t pid;    // Product ID
        uint8_t  cd[16]; // Compile Date
        uint8_t  mn[16]; // Manufacturer Name
        uint8_t  pn[16]; // Production Name
        uint8_t  sn[16]; // Serial Number
        uint8_t  rsv[14];
        uint8_t test_cnt;
        uint8_t crc8;
    } pi;

    // power management
    struct {
        uint8_t en;
        uint16_t time;
        uint8_t rsv[1+16];
    } pm;

    // function selection
    struct __attribute__((packed)) {
        uint8_t  nkro : 1;
        uint8_t  auto_calib : 1;
        uint8_t  startup_effect : 1; // startup light effect
        uint8_t  rt_separately : 1;
        uint8_t  cc_popup : 1; // startup control center popup
        uint8_t  rsv : 3;

        uint8_t  gp_mode; // off / xbox / hid
        uint8_t  link_mode;  // USB, Bluetooth and Radio

        uint16_t scan_rate;
        uint16_t report_rate;

        uint8_t  dks_tick;
        uint8_t  input_tick;
        uint8_t  debounce_tick;

        uint8_t  rsv1[14];
    } fs;
} __attribute__ ((aligned (4))) obi_t;


extern obi_t g_obi;
extern obkp_t g_obkp[KBD_KEY_MAX];


rotsta_t keyprop_init(void);


#endif  /* KEY_ATTR_H */
