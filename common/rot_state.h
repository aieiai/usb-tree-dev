#ifndef ROT_STATE_H
#define ROT_STATE_H

#include <stdint.h>

typedef uint32_t rot_state_t;


/** @brief Status code group definition
 *  @rule:
 *      [Group]: 
 *      [Code] :  
 */
#define ROT_STATE_MAKE(group, code) ((uint32_t)(group)*512U + (uint32_t)(code))
// #define ROT_STATE_CHECK(state) (state_success != (state))


enum {
    rot_state_group_common = 0,
    rot_state_group_flash,
    rot_state_group_fdb,
    rot_state_group_hal_cc,
    rot_state_group_rotcc,
    rot_state_group_rotdt,
    rot_state_group_upgrade,
};


// General status code
enum {
    rot_state_success = ROT_STATE_MAKE(rot_state_group_common, 0),
    rot_state_fail,
    rot_state_timeout,
    rot_state_invalid_argument,
    rot_state_not_supported,
    rot_state_not_found,
};


#endif
