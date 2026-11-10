#ifndef ROT_VER_H
#define ROT_VER_H

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

#define ROT_VER       100
#define ROT_VER_MAJOR 1
#define ROT_VER_MINOR 0
#define ROT_VER_PATCH 0
#define ROT_VER_STAGE ROT_VER_STAGE_DEV


#define ROT_VER_STAGE_RELEASE 0
#define ROT_VER_STAGE_DEV     1
#define ROT_VER_STAGE_ALPHA   2
#define ROT_VER_STAGE_BETA    3
#define ROT_VER_STAGE_RC      4


// 构建 HASH：由 CMake 构建时注入 -DROT_GIT_HASH=0x????????；单独编译时默认为 0x01234567
#ifndef ROT_GIT_HASH
#define ROT_GIT_HASH 0x01234567U
#endif

#define ROT_VER_MAKE(major, minor, patch, stage, hash)          \
    ((((uint64_t)(major) & 0x0FFULL) << 56) |                   \
     (((uint64_t)(minor) & 0x3FFULL) << 46) |                   \
     (((uint64_t)(patch) & 0x3FFULL) << 36) |                   \
     (((uint64_t)(stage) & 0x00FULL) << 32) |                   \
     ((uint64_t)(hash)   & 0xFFFFFFFFULL))

#define FW_VER ROT_VER_MAKE(ROT_VER_MAJOR, ROT_VER_MINOR, ROT_VER_PATCH, ROT_VER_STAGE, ROT_GIT_HASH)

#define ROT_VER_GET_MAJOR(v) ((unsigned)(((uint64_t)(v) >> 56) & 0xFFULL))
#define ROT_VER_GET_MINOR(v) ((unsigned)(((uint64_t)(v) >> 46) & 0x3FFULL))
#define ROT_VER_GET_PATCH(v) ((unsigned)(((uint64_t)(v) >> 36) & 0x3FFULL))
#define ROT_VER_GET_STAGE(v) ((unsigned)(((uint64_t)(v) >> 32) & 0x0FULL))
#define ROT_VER_GET_HASH(v)  ((uint32_t)((uint64_t)(v) & 0xFFFFFFFFULL))

static inline const char *rot_ver_stage_str(unsigned stage) {
    static const char *const names[] = { "release", "dev", "alpha", "beta", "rc" };
    return (stage < (sizeof(names) / sizeof(names[0])) ? names[stage] : "unknown");
}

#endif // ROT_VER_H
