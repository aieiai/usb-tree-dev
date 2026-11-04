#ifndef FDB_H
#define FDB_H

#include <stdint.h>
#include <string.h>
#include "rot_state.h"


enum {
    rot_state_fdb_init_fail = ROT_STATE_MAKE(rot_state_group_fdb, 1),
    rot_state_fdb_not_init,
    rot_state_fdb_invalid_type,
    rot_state_fdb_save_fail,
    rot_state_fdb_lock_fail,
};

typedef enum {
    FDB_TYPE_FACTORY = 0,
    FDB_TYPE_CALIB,
    FDB_TYPE_COMMON,
    FDB_TYPE_BIF,
    FDB_TYPE_MAP,
    FDB_TYPE_ADV,
    FDB_TYPE_RGB,
    FDB_TYPE_RSV2,
    FDB_TYPE_MACRO,
    FDB_TYPE_COUNT,
} fdb_type_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t name[16];
} __attribute__((aligned(4))) fdb_head_t;

typedef struct {
    fdb_head_t header;
} __attribute__((aligned(4))) fdb_fac_t;

typedef struct {
    fdb_head_t header;
} __attribute__((aligned(4))) fdb_calib_t;

typedef struct {
    fdb_head_t header;
} __attribute__((aligned(4))) fdb_common_t;

typedef struct {
    fdb_head_t header;
    uint8_t gp_mode;
} __attribute__((aligned(4))) fdb_bif_t;

typedef struct {
    fdb_head_t header;
} __attribute__((aligned(4))) fdb_map_t;

typedef struct {
    fdb_head_t header;
} __attribute__((aligned(4))) fdb_adv_t;

typedef struct {
    fdb_head_t header;
} __attribute__((aligned(4))) fdb_rgb_t;

typedef struct {
    fdb_head_t header;
} __attribute__((aligned(4))) fdb_rsv2_t;

typedef struct {
    fdb_head_t header;
} __attribute__((aligned(4))) fdb_macro_t;

typedef struct {
    fdb_fac_t     fac;
    fdb_calib_t   calib;
    fdb_common_t  common;
    fdb_bif_t     bif;
    fdb_map_t     map;
    fdb_adv_t     adv;
    fdb_rgb_t     rgb;
    fdb_rsv2_t    rsv2;
    fdb_macro_t   macro;
} fdb_data_t;

extern fdb_data_t fdb_data;

/*
 * Initialization is boot-only. At runtime, callers update fields in fdb_data
 * and call fdb_update() for the corresponding type.
 * Example: fdb_data.bif.gp_mode = mode; (void)fdb_update(FDB_TYPE_BIF);
 *
 * FDB_TYPE_FACTORY, FDB_TYPE_CALIB and FDB_TYPE_BIF are saved immediately.
 * Other types are saved after 5 seconds without another update.
 * fdb_update() must be called from thread context. Immediate types block until
 * the save completes; delayed types return after their snapshot is queued.
 */
rot_state_t fdb_init(void);
rot_state_t fdb_update(fdb_type_t type);


#endif
