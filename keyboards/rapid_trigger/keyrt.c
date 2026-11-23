/**
 * @Author: wj
 * @Brief:  key rapid trigger.
 * @Date:   2024/07/02
 */
#include "keyrt.h"
#include "keyevt.h"
#include "keyprop.h"
#include "kbd_config.h"

#define S16_MIN     ( 0x8000 )
#define S16_MAX     ( 0x7FFF )


typedef enum {
    KRT_EVT_ENT_U2U = 0,
    KRT_EVT_ENT_D2D,
} keyrt_evt_ent_t;

typedef enum {
    KRT_EVT_TRG_U2U = 0,
    KRT_EVT_TRG_D2U,
    KRT_EVT_TRG_D2D,
    KRT_EVT_TRG_U2D,
} keyrt_evt_trg_t;

typedef enum {
    KRT_PLS_MD_RDT = 0,
    KRT_PLS_MD_MAX,
} keyrt_pls_type_t;


typedef struct _key_rt {
    bool rst;
    keyrt_evt_ent_t evt_ent;
    keyrt_evt_trg_t evt_trg;

    int16_t dst_pd;      /* press depth dist */
    int16_t dst_rd;      /* raise depth dist */
    int16_t dst_pnm;     /* press need move dist */
    int16_t dst_rnm;     /* raise need move dist */
} __attribute__((aligned(4))) keyrt_t;



static keyrt_t keyrt[KBD_KEY_MAX];


/**
 * @brief:  Record the release depth dist
 * @param:  The index of the key
 * @param:  The current depth dist
 * @return: None
 *
 *          If the current depth dist is smaller than the previous one, the
 *          previous one will be updated.
 */
static inline void _record_depth_release_dst(uint8_t ki, int16_t cur_dst)
{
    keyrt_t *p_rt = &keyrt[ki];
    const keyaxis_para_t *p_axis = g_keyevt[ki].axis_para;

    if (p_rt->dst_rd <= cur_dst) {
        return;    
    }
    float dst     = cur_dst + g_obkp.attr[ki].rt.dst_p;
    p_rt->dst_rd  = cur_dst;
    p_rt->dst_pd  = S16_MIN;
    p_rt->dst_pnm = (dst > p_axis->t2b_dst) ? p_axis->t2b_dst : dst;
    p_rt->dst_rnm = 0;
}

/**
 * @brief:  Record the press depth dist
 * @param:  The index of the key
 * @param:  The current depth dist
 * @return: None
 *
 *          If the current depth dist is smaller than the previous one, the
 *          previous one will be updated.
 */
static inline void _record_depth_press_dst(uint8_t ki, int16_t cur_dst)
{
    keyrt_t *p_rt = &keyrt[ki];
    if (p_rt->dst_pd >= cur_dst) {
        return;
    }
    int16_t dst   = cur_dst - g_obkp.attr[ki].rt.dst_r;
    p_rt->dst_rd  = cur_dst;
    p_rt->dst_pd  = S16_MAX;
    p_rt->dst_rnm = (dst < 0) ? 0 : dst;
    p_rt->dst_rnm = 0;
}


/**
 * @brief:  Process the key rapid trigger event.
 * @param:  The index of the key.
 * @return: None
 *
 *          This function is used to process the key rapid trigger event.
 *          If the key is pressed from up to up, the keyrt.rst will be set to true.
 *          If the key is pressed from up to down, the keyrt.evt_ent will be set to
 *          KRT_EVT_ENT_D2D, and the keyrt.evt_trg will be set to KRT_EVT_TRG_U2D.
 *          The press depth dist will be recorded.
 *          If the key is pressed from down to down, the keyrt.evt_trg will be set to
 *          KRT_EVT_TRG_D2D, and the raise depth dist will be recorded.
 *          If the key is pressed from down to up, the keyrt.evt_trg will be set to
 *          KRT_EVT_TRG_D2U, and the press depth dist will be recorded.
 */
void keyrt_process(uint8_t ki)
{
    keyrt_t *p_rt = &keyrt[ki];
    if (p_rt->evt_ent == KRT_EVT_ENT_U2U) {
        keyrt_reset(ki);
        return;
    }

    int16_t cur_dst = g_keyevt[ki].dst;
    if (p_rt->evt_ent == KRT_EVT_ENT_U2U) {
        p_rt->rst     = true;
        p_rt->evt_ent = KRT_EVT_ENT_D2D;
        p_rt->evt_trg = KRT_EVT_TRG_U2D;
        _record_depth_press_dst(ki, cur_dst);
        return;
    }
    
    if (p_rt->evt_trg < KRT_EVT_TRG_D2D) {
        _record_depth_release_dst(ki, cur_dst);
        p_rt->evt_trg = (p_rt->dst_pnm <= cur_dst) ? KRT_EVT_TRG_U2D : KRT_EVT_TRG_U2U;
    }
    else {
        _record_depth_press_dst(ki, cur_dst);
        p_rt->evt_trg = (p_rt->dst_rnm >= cur_dst) ? KRT_EVT_TRG_D2U : KRT_EVT_TRG_D2D;
    }
}



/**
 * @brief:  Reset the key rapid trigger state.
 * @param:  The index of the key.
 * @return: None
 *
 *          Reset the key rapid trigger state to uninitialized.
 */
void keyrt_reset(uint8_t ki)
{
    keyrt_t *p_rt = &keyrt[ki];
    if (p_rt->rst == false) {
        return;
    }
    p_rt->rst     = false;
    p_rt->dst_pd  = S16_MIN;
    p_rt->dst_rd  = S16_MAX;
    p_rt->dst_rnm = 0;
    p_rt->dst_pnm = 0;
    p_rt->evt_ent = KRT_EVT_ENT_U2U;
    p_rt->evt_trg = KRT_EVT_TRG_U2U;
}

void keyrt_reset_force(uint8_t ki)
{
    keyrt[ki].rst = true;
    keyrt_reset(ki);
}

/**
 * @brief:  Initialize the key rapid trigger module.
 * @param:  None
 * @return: The pointer to the key rapid trigger module.
 *
 *          This function is used to initialize the key rapid trigger module.
 *          It will reset all the key's rapid trigger state to uninitialized.
 */
void keyrt_init(void)
{
    for (uint8_t ki = 0; ki < KBD_KEY_MAX; ki++) {
        keyrt_reset_force(ki);
    }
}
