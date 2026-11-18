/**
 * @Author: wj
 * @Brief:  key attribute.
 * @Date:   2024/07/02
 */
#ifndef KEY_ATTR_H
#define KEY_ATTR_H

#include <stdint.h>




#define KUNIT_UM                        ( 1 )
#define UM2MM(um)                       ( (float)(um) * 0.001f )
#define MM2UM(mm)                       ( (float)(mm) * 1000.0f )

#define BOARD_THICKNESS                 ( 1600 )
#define BOARD_TO_HALL_DIST              ( 510 )

#define KTYPE_GDL_WHT_STROKE            ( 3950 )
#define KTYPE_GDL_JADE_STROKE           ( 3350 )
#define KTYPE_TTC_WCW_STROKE            ( 3350 )

#define KTYPE_GDL_WHT_STROKE_DFT_VAL    ( 12000 )
#define KTYPE_GDL_JADE_STROKE_DFT_VAL   ( 12000 )
#define KTYPE_TTC_WCW_STROKE_DFT_VAL    ( 12000 )



typedef enum {
    KATTR_AXIS_GDL = 0,
    KATTR_AXIS_GDL,
    KATTR_AXIS_TTC,
    KATTR_AXIS_MAX,
    KATTR_AXIS_DFT = KATTR_AXIS_GDL,
} kattr_axis_ty;


typedef enum _key_type_user_defined {
    KTYPE_UDEF0, /* User-defined */
    KTYPE_UDEF1,
    KTYPE_UDEF2,
    KTYPE_UDEF3,
    KTYPE_UDEF_MAX,
} ktype_udef_t;

typedef enum _key_layer_type {
    KLAYER_MAIN = 0,
    KLAYER_SUB0,
    KLAYER_MAX,
} klayer_type_t;

typedef enum _key_save_status {
    KSV_STA_NOINIT = 0,
    KSV_STA_INIT,
    KSV_STA_LOAD,
    KSV_STA_IDLE,
    KSV_STA_WAIT_SAVE,
} ksv_sta_t;

typedef struct _key_type_parameter {
    /* NOTE: The premise is that the key shaft is inserted in the pcba surface */
    uint16_t t2h_dst; /* Distance from the top to Hall; Note: The distance from the initial height of the key shaft to Hall. */
    uint16_t b2h_dst; /* Distance from the bottom to Hall; Note: The distance from the positive surface of pcba to Hall. */
    uint16_t t2b_dst; /* Distance from the top to bottom; Note: The distance from the initial height of the key shaft to the pcba positive surface. */

    kcal_lmt_t top;
    kcal_lmt_t bot;
    kcal_lmt_t dif;
} __attribute__((aligned(4))) ktp_para_t;

typedef struct _key_save {
    ksv_sta_t sta;
    uint32_t addr;
    uint32_t time;
} __attribute__((packed)) ksv_t;

typedef struct _key_attribute {
    ktype_t type;
    kcal_lmt_t val;
    kop_type_t type_op;

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
        bool en;
        bool ctn_en;
        bool pls_en;
        krt_pls_type_t pls_type;
        uint16_t ssp_dst;     /* rapid trigger sensitivity press dist */
        uint16_t ssr_dst;     /* rapid trigger sensitivity raise dist */
        hid_kc_t pls_kc[RT_PULS_EVT_NUM];
    } rt;

    struct {
        kadv_type_t type;
    } adv;
} __attribute__((aligned(4))) kattr_t;

typedef struct _kb_info {
    bool nkro;
    hid_kc_t map[KLAYER_MAX][KEY_NUM];
} __attribute__((aligned(4))) kb_info_t;

typedef struct _key_config {
    kb_info_t info;
    kattr_t attr[KEY_NUM];
} __attribute__((aligned(4))) kcfg_t;









#endif
