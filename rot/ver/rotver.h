#ifndef ROTVER_H
#define ROTVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

/**
 * @brief 版本号使用 64bit 位定义与解析逻辑：
 *   bit 63..56  major          8  bit
 *   bit 55..46  minor          10 bit
 *   bit 45..36  patch          10 bit
 *   bit 35..32  stage          4  bit (0:release, 1:dev, 2:alpha, 3:beta, 4:rc)
 *   bit 31..0    hash          32 bit
 *   e.g.        v1.0.0-release+01234567 -> 0x0100000001234567
 */

// v0.1.0-dev
#define ROTVER_MAJOR 0
#define ROTVER_MINOR 1
#define ROTVER_PATCH 0
// "release", "dev", "alpha", "beta", "rc"
#define ROTVER_STAGE "release"

#define FW_VER ROTVER_MAKE(ROTVER_MAJOR, ROTVER_MINOR, ROTVER_PATCH, rotver_stage_num(ROTVER_STAGE), ROTVER_GIT_HASH)

enum {
    ROTVER_STAGE_RELEASE = 0,
    ROTVER_STAGE_DEV,
    ROTVER_STAGE_ALPHA,
    ROTVER_STAGE_BETA,
    ROTVER_STAGE_RC,
    ROTVER_STAGE_MAX,
    ROTVER_STAGE_MASK = 0x0F,
};


// 构建 HASH：由 CMake 构建时注入 -DROTVER_GIT_HASH=0x????????；单独编译时默认为 0x01234567
#ifndef ROTVER_GIT_HASH
#define ROTVER_GIT_HASH 0x01234567U
#endif

#ifndef ROTVER_GIT_HASH_STR
#define ROTVER_GIT_HASH_STR "01234567"
#endif

#define ROTVER_MAKE(major, minor, patch, stage, hash)   \
    ((((uint64_t)(major) & 0x0FFULL) << 56) |           \
     (((uint64_t)(minor) & 0x3FFULL) << 46) |           \
     (((uint64_t)(patch) & 0x3FFULL) << 36) |           \
     (((uint64_t)(stage) & ROTVER_STAGE_MASK) << 32) |           \
     ((uint64_t)(hash)   & 0xFFFFFFFFULL))


#define ROTVER_GET_MAJOR(v) ((unsigned)(((uint64_t)(v) >> 56) & 0xFFULL))
#define ROTVER_GET_MINOR(v) ((unsigned)(((uint64_t)(v) >> 46) & 0x3FFULL))
#define ROTVER_GET_PATCH(v) ((unsigned)(((uint64_t)(v) >> 36) & 0x3FFULL))
#define ROTVER_GET_STAGE(v) ((unsigned)(((uint64_t)(v) >> 32) & ROTVER_STAGE_MASK))
#define ROTVER_GET_HASH(v)  ((uint32_t)((uint64_t)(v) & 0xFFFFFFFFULL))

static inline const char *rotver_stage_str(unsigned stage) {
    const char *const names[ROTVER_STAGE_MAX] = { "release", "dev", "alpha", "beta", "rc" };
    return (stage < ROTVER_STAGE_MAX ? names[stage] : "unknown");
}

static inline const uint8_t rotver_stage_num(char *name) {
    const char *const names[ROTVER_STAGE_MAX] = { "release", "dev", "alpha", "beta", "rc" };
    for (uint8_t i = 0; i < ROTVER_STAGE_MAX; i++) {
        if (strcmp(names[i], name) == 0) {
            return i;
        }
    }
    return ROTVER_STAGE_MASK;
}


#ifdef __cplusplus
}
#endif

#endif // ROTVER_H
