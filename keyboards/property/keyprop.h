/**
 * @Author: wj
 * @Brief:  key property.
 * @Date:   2024/07/02
 */
#ifndef KEY_PROPERTY_H
#define KEY_PROPERTY_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "rotsta.h"

#include "kbd_config.h"
#include "keymagnet.h"
#include "keyaxis.h"
#include "keycode.h"
#include "keycore.h"

#define MAGIC_OBKP 0x79890001U /* OBKP */
#define OBKP_VER 0U


#define OBKP_DFT_TRG_TY             (KTRG_TY_NOM)
#define OBKP_DFT_AXIS_TY            (KAXIS_BB)
#define OBKP_DFT_AXIS_UDEF_TY       (KAXIS_UDEF0)

#define OBKP_DFT_ACT_SCLT           400
#define OBKP_DFT_ACT_SCLTO          200
#define OBKP_DFT_ACT_DCLT           400
#define OBKP_DFT_ACT_DCLTO          200

#define OBKP_DFT_DZ_DST_T           MM2UM(0.0f)
#define OBKP_DFT_DZ_DST_B           MM2UM(0.0f)
#define OBKP_DFT_NOM_DST_P          MM2UM(1.0f)
#define OBKP_DFT_NOM_DST_R_OFFSET   MM2UM(-0.1f)
#define OBKP_DFT_NOM_DST_R          (OBKP_DFT_NOM_DST_P + OBKP_DFT_NOM_DST_R_OFFSET)
#define OBKP_DFT_RT_DST_P           MM2UM(0.2f)
#define OBKP_DFT_RT_DST_R           MM2UM(0.2f)


typedef enum {
    KPROP_LINK_USB = 0,
    KPROP_LINK_BT,
    KPROP_LINK_RADIO,
    KPROP_LINK_MAX
} kprop_link_ty;

typedef enum {
    KPROP_GP_OFF = 0,
    KPROP_GP_HID,
    KPROP_GP_XBOX,
    KPROP_GP_MAX
} kprop_gp_ty;

typedef enum {
    KTRG_TY_NOM = 0,
    KTRG_TY_RT,
    KTRG_TY_RT_CTN,
    KTRG_TY_MAX
} ktrg_ty;

typedef struct {
    uint32_t crc32;
    ktrg_ty trg_ty;

    struct __attribute__((packed)) {
        kaxis_ty ty;
        kaxis_udef_ty udef_ty;
    } axis;
    struct __attribute__((packed)) {
        uint16_t sclt;
        uint16_t sclto;
        uint16_t dclt;
        uint16_t dclto;
    } act;
    struct __attribute__((packed)) {
        uint16_t dst_t;     /* top dead zero dist */
        uint16_t dst_b;     /* bot dead zero dist */
    } dz;
    struct __attribute__((packed)) {
        uint16_t dst_p;      /* normal press dist */
        uint16_t dst_r;      /* normal raise dist */
    } nom;
    struct __attribute__((packed)) {
        uint16_t dst_p;     /* rapid trigger sensitivity press dist */
        uint16_t dst_r;     /* rapid trigger sensitivity raise dist */
    } rt;
    struct __attribute__((packed)) {
        // kadv_ty type;
        uint8_t tmp;
    } adv;
} __attribute__((aligned(4))) keyattr_t;

// on-board key property
typedef struct {
    uint32_t magic;
    uint32_t crc32;
    uint32_t version;
    uint8_t  name[32];

    keyattr_t attr[KBD_KEY_MAX];
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

        kprop_gp_ty   gp_mode;
        kprop_link_ty link_mode;

        uint16_t scan_rate;
        uint16_t report_rate;

        uint8_t  dks_tick;
        uint8_t  input_tick;
        uint8_t  debounce_tick;

        uint8_t  rsv1[14];
    } fs;
} __attribute__ ((aligned (4))) obi_t;


extern obi_t g_obi;
extern obkp_t g_obkp;

rotsta_t obi_db_register(void);
rotsta_t obkp_db_register(void);
rotsta_t keyprop_init(void);

bool keyprop_cc_popup_enabled(void);


#endif  /* KEY_PROPERTY_H */
