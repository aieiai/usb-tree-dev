#ifndef AXIS_CONFIG_H
#define AXIS_CONFIG_H

#include "keymagnet.h"

#define KAXIS_BB_STROKE             ( 3500 )
#define KAXIS_GDL_STROKE            ( 3950 )
#define KAXIS_TTC_STROKE            ( 3350 )

#define KAXIS_BB_STROKE_DFT_VAL     ( 12000 )
#define KAXIS_GDL_STROKE_DFT_VAL    ( 12000 )
#define KAXIS_TTC_STROKE_DFT_VAL    ( 12000 )


typedef enum {
    KAXIS_BB = 0,
    KAXIS_GDL,
    KAXIS_TTC,
    KAXIS_MAX,
    KAXIS_DFT = KAXIS_GDL,
} kaxis_ty;

typedef enum {
    KAXIS_UDEF0, /* User-defined */
    KAXIS_UDEF1,
    KAXIS_UDEF2,
    KAXIS_UDEF3,
    KAXIS_UDEF_MAX,
} kaxis_udef_ty;

typedef struct {
    /* NOTE: The premise is that the key shaft is inserted in the pcba surface */
    uint16_t t2h_dst; /* Distance from the top to Hall; Note: The distance from the initial height of the key shaft to Hall. */
    uint16_t b2h_dst; /* Distance from the bottom to Hall; Note: The distance from the positive surface of pcba to Hall. */
    uint16_t t2b_dst; /* Distance from the top to bottom; Note: The distance from the initial height of the key shaft to the pcba positive surface. */

    keymag_lmt_t top;
    keymag_lmt_t bot;
    keymag_lmt_t dif;
} __attribute__((aligned(4))) keyaxis_para_t;

extern const keyaxis_para_t g_keyaxis_para[KAXIS_MAX];

#endif  /* AXIS_CONFIG_H */
