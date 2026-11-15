#ifndef FDB_H
#define FDB_H

#include <stdint.h>
#include "rotsta.h"


#define FDB_SOBI_MAGIC  (0x534F4249U) /* "SOBI" */
#define FDB_SOBI_VER    (2U)

enum {
    rotsta_fdb_init_fail = ROTSTA_MAKE(rotsta_group_fdb, 1),
    rotsta_fdb_not_init,
    rotsta_fdb_invalid_type,
    rotsta_fdb_not_registered,
    rotsta_fdb_invalid_save_type,
    rotsta_fdb_save_fail,
    rotsta_fdb_lock_fail,
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

    
    // product information
    struct __attribute__((packed)) {
        uint16_t vid;    // Vendor ID
        uint16_t pid;    // Product ID
        uint64_t fv;     // Firmware Version
        uint64_t hv;     // Hardware Version
        uint8_t  cd[16]; // Compile Date
        uint8_t  mn[16]; // Manufacturer Name
        uint8_t  pn[16]; // Production Name
        uint8_t  sn[16]; // Serial Number
        uint8_t  rsv[14];
        uint8_t test_cnt;
        uint8_t crc8;
    } pi;

} __attribute__((aligned(4))) sobi_t;

extern sobi_t g_sobi;


rotsta_t fdb_init(void);
rotsta_t fdb_register(fdb_item_ty type, void *buf, uint32_t buf_size);
rotsta_t fdb_update(fdb_item_ty type, fdb_save_ty save_type);

rotsta_t fdb_sobi_init(void);


#endif
