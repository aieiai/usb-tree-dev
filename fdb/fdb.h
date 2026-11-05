#ifndef FDB_H
#define FDB_H

#include <stdint.h>
#include "rot_state.h"


enum {
    rot_state_fdb_init_fail = ROT_STATE_MAKE(rot_state_group_fdb, 1),
    rot_state_fdb_not_init,
    rot_state_fdb_invalid_type,
    rot_state_fdb_not_registered,
    rot_state_fdb_invalid_save_type,
    rot_state_fdb_save_fail,
    rot_state_fdb_lock_fail,
};

typedef enum {
    FDB_ITEM_TY_FAC = 0,
    FDB_ITEM_TY_CALIB,
    FDB_ITEM_TY_COMMON,
    FDB_ITEM_TY_SOBI,
    FDB_ITEM_TY_MAP,
    FDB_ITEM_TY_ADV,
    FDB_ITEM_TY_RGB,
    FDB_ITEM_TY_MACRO,
    FDB_ITEM_TY_MAX,
} fdb_item_ty;

typedef enum {
    FDB_SAVE_TY_NOW = 0,
    FDB_SAVE_TY_D5S,
    FDB_SAVE_TY_MAX,
} fdb_save_ty;


// System on-board information
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t  name[32];

    uint8_t  gp_mode;
    uint8_t  sys_mode;

    struct __attribute__((packed)) {
        uint8_t cc_popup : 1;
    } en;


} __attribute__((aligned(4))) sobi_t;

extern sobi_t g_sobi;


rot_state_t fdb_init(void);
rot_state_t fdb_register(fdb_item_ty type, void *buf, uint32_t buf_size);
rot_state_t fdb_update(fdb_item_ty type, fdb_save_ty save_type);


#endif
