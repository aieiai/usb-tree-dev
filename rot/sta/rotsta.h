#ifndef ROTSTA_H
#define ROTSTA_H

#include <stdint.h>

typedef uint32_t rotsta_t;


/** @brief Status code group definition
 *  @rule:
 *      [Group]: 
 *      [Code] :  
 */
#define ROTSTA_MAKE(group, code) ((uint32_t)(group)*512U + (uint32_t)(code))
// #define ROTSTA_CHECK(state) (state_success != (state))


enum {
    rotsta_group_common = 0,
    rotsta_group_flash,
    rotsta_group_fdb,
    rotsta_group_hal_cc,
    rotsta_group_rotcc,
    rotsta_group_rotdt,
    rotsta_group_upgrade,
};


// General status code
enum {
    rotsta_success = ROTSTA_MAKE(rotsta_group_common, 0),
    rotsta_fail,
    rotsta_timeout,
    rotsta_invalid_argument,
    rotsta_not_supported,
    rotsta_not_found,
};


#endif
