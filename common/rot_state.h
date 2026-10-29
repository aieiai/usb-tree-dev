#ifndef ROT_STATUE_CODE_H
#define ROT_STATUE_CODE_H

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
};


enum {
    rot_state_success = ROT_STATE_MAKE(rot_state_group_common, 0),
    rot_state_fail,
    rot_state_timeout,
    rot_state_invalid_argument,
    rot_state_not_supported,
};






#endif
