#ifndef ROTVER_H
#define ROTVER_H

#include <stdint.h>

/**
 * @brief 版本号使用 64bit 位定义与解析逻辑：
 *   bit 63..56  major          8  bit
 *   bit 55..46  minor          10 bit
 *   bit 45..36  patch          10 bit
 *   bit 35..32  stage          4  bit (0:release, 1:dev, 2:alpha, 3:beta, 4:rc)
 *   bit 31..0    hash          32 bit
 *   e.g.        v1.0.0-release+01234567 -> 0x0100000001234567
 */

#define ROTVER       100
#define ROTVER_MAJOR 1
#define ROTVER_MINOR 0
#define ROTVER_PATCH 0
#define ROTVER_STAGE ROTVER_STAGE_DEV


#define ROTVER_STAGE_RELEASE 0
#define ROTVER_STAGE_DEV     1
#define ROTVER_STAGE_ALPHA   2
#define ROTVER_STAGE_BETA    3
#define ROTVER_STAGE_RC      4


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
     (((uint64_t)(stage) & 0x00FULL) << 32) |           \
     ((uint64_t)(hash)   & 0xFFFFFFFFULL))

#define FW_VER ROTVER_MAKE(ROTVER_MAJOR, ROTVER_MINOR, ROTVER_PATCH, ROTVER_STAGE, ROTVER_GIT_HASH)

#define ROTVER_GET_MAJOR(v) ((unsigned)(((uint64_t)(v) >> 56) & 0xFFULL))
#define ROTVER_GET_MINOR(v) ((unsigned)(((uint64_t)(v) >> 46) & 0x3FFULL))
#define ROTVER_GET_PATCH(v) ((unsigned)(((uint64_t)(v) >> 36) & 0x3FFULL))
#define ROTVER_GET_STAGE(v) ((unsigned)(((uint64_t)(v) >> 32) & 0x0FULL))
#define ROTVER_GET_HASH(v)  ((uint32_t)((uint64_t)(v) & 0xFFFFFFFFULL))

static inline const char *rotver_stage_str(unsigned stage) {
    static const char *const names[] = { "release", "dev", "alpha", "beta", "rc" };
    return (stage < (sizeof(names) / sizeof(names[0])) ? names[stage] : "unknown");
}

#endif // ROTVER_H
