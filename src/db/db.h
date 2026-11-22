#ifndef DB_H
#define DB_H

#include <stdint.h>
#include "rotsta.h"


#define MAGIC_OBI  (0x534F4249U) /* "OBI" */
#define OBI_VER    (1U)

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
    DB_ITEM_TY_OBF = 0,
    DB_ITEM_TY_OBMAG,
    DB_ITEM_TY_OBCOM,
    DB_ITEM_TY_OBI,
    DB_ITEM_TY_OBM,
    DB_ITEM_TY_OBA,
    DB_ITEM_TY_OBR,
    DB_ITEM_TY_OBKP,
    DB_ITEM_TY_MAX,
} db_item_ty;

typedef enum {
    DB_SAVE_TY_NOW = 0,
    DB_SAVE_TY_D5S,
    DB_SAVE_TY_MAX,
} db_save_ty;



rotsta_t db_init(void);
rotsta_t db_register(db_item_ty type, void *buf, uint32_t buf_size);
rotsta_t db_update(db_item_ty type, db_save_ty save_type);



#endif
