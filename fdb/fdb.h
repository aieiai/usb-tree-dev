#ifndef FDB_H
#define FDB_H

#include <stdint.h>
#include <string.h>
#include "rot_state.h"


enum {
    rot_state_fdb_init_fail = ROT_STATE_MAKE(rot_state_group_fdb, 1),
};

#endif
