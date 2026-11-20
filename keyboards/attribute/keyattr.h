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

#include "kbdconfig.h"
#include "keymagnet.h"
#include "keyaxis.h"
#include "keycode.h"


typedef enum {
    KTRG_TY_NOM = 0,
    KTRG_TY_RT,
    KTRG_TY_RT_CTN,
    KTRG_TY_ADV,
} ktrg_ty;

typedef struct {
    /* NOTE: The premise is that the key shaft is inserted in the pcba surface */
    uint16_t t2h_dst; /* Distance from the top to Hall; Note: The distance from the initial height of the key shaft to Hall. */
    uint16_t b2h_dst; /* Distance from the bottom to Hall; Note: The distance from the positive surface of pcba to Hall. */
    uint16_t t2b_dst; /* Distance from the top to bottom; Note: The distance from the initial height of the key shaft to the pcba positive surface. */

    kmag_lmt_t top;
    kmag_lmt_t bot;
    kmag_lmt_t dif;
} __attribute__((aligned(4))) kattr_para_t;


// on-board key property
typedef struct {
    uint32_t magic;
    uint32_t crc32;

    kaxis_ty axis;
    kaxis_udef_ty axis_udef;

    ktrg_ty trg_ty;
    kmag_lmt_t val;
    // kop_type_t type_op;

    struct {
        uint16_t sclt;
        uint16_t sclto;
        uint16_t dcit;
        uint16_t dclt;
        uint16_t dclto;
    } tk; /* tick */

    struct {
        uint16_t p_dst;      /* normal press dist */
        uint16_t r_dst;      /* normal raise dist */
    } nom;

    struct {
        uint16_t top_dst;     /* top dead zero dist */
        uint16_t bot_dst;     /* bot dead zero dist */
    } dz;

    struct {
        uint8_t en : 1;
        uint8_t ctn_en : 1;
        uint8_t pls_en : 1;
        uint8_t rsv : 5;
        uint8_t pls_ty;
        uint16_t ssp_dst;     /* rapid trigger sensitivity press dist */
        uint16_t ssr_dst;     /* rapid trigger sensitivity raise dist */
        rotkc_t pls_kc[4];
    } rt;

    // struct {
    //     kadv_ty type;
    // } adv;
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
extern obkp_t g_obp[KBD_KEY_MAX];


rotsta_t keyattr_init(void);


#endif  /* KEY_ATTR_H */
