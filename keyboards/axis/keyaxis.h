#ifndef AXIS_CONFIG_H
#define AXIS_CONFIG_H


#define KAXIS_GDLSTROKE             ( 3950 )
#define KAXIS_TTC__STROKE           ( 3350 )

#define KAXIS_GDL_STROKE_DFT_VAL    ( 12000 )
#define KAXIS_TTC_STROKE_DFT_VAL    ( 12000 )


typedef enum {
    KAXIS_GDL = 0,
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



#endif  /* AXIS_CONFIG_H */
