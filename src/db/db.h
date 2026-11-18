#ifndef DB_H
#define DB_H

#include <stdint.h>
#include "rotsta.h"


#define DB_SOBI_MAGIC  (0x534F4249U) /* "SOBI" */
#define DB_SOBI_VER    (1U)

enum {
    rotsta_db_init_fail = ROTSTA_MAKE(rotsta_group_db, 1),
    rotsta_db_not_init,
    rotsta_db_invalid_type,
    rotsta_db_not_registered,
    rotsta_db_invalid_save_type,
    rotsta_db_save_fail,
    rotsta_db_lock_fail,
};

typedef enum {
    DB_ITEM_TY_FAC = 0,
    DB_ITEM_TY_CALIB,
    DB_ITEM_TY_COMMON,
    DB_ITEM_TY_SOBI,
    DB_ITEM_TY_MAP,
    DB_ITEM_TY_ADV,
    DB_ITEM_TY_RGB,
    DB_ITEM_TY_MACRO,
    DB_ITEM_TY_MAX,
} db_item_ty;

typedef enum {
    DB_SAVE_TY_NOW = 0,
    DB_SAVE_TY_D5S,
    DB_SAVE_TY_MAX,
} db_save_ty;


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


rotsta_t db_init(void);
rotsta_t db_register(db_item_ty type, void *buf, uint32_t buf_size);
rotsta_t db_update(db_item_ty type, db_save_ty save_type);

rotsta_t db_sobi_init(void);


#endif
