#include "rotver.h"


const char *rotver_get_cd(void)
{
    return ROTVER_BUILD_TIME;
}

uint8_t rotver_get_stage_num(char *name)
{
    const char *const names[ROTVER_STAGE_MAX] = { "release", "dev", "alpha", "beta", "rc" };
    for (uint8_t i = 0; i < ROTVER_STAGE_MAX; i++) {
        if (strcmp(names[i], name) == 0) {
            return i;
        }
    }
    return ROTVER_STAGE_MASK;
}

const char *rotver_get_stage_str(unsigned stage)
{
    const char *const names[ROTVER_STAGE_MAX] = { "release", "dev", "alpha", "beta", "rc" };
    return (stage < ROTVER_STAGE_MAX ? names[stage] : "unknown");
}
