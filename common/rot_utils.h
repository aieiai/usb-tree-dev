#ifndef ROT_UTILS_H
#define ROT_UTILS_H


#define ROT_DIV_CEIL(size, align)               (((size) + (align) - 1) / (align))
#define ROT_DIV_ROUND_NEAREST(size, align)      (((size) + (align)/2) / (align) ) // round to nearest integer
#define ROT_ALIGN_UP(size, align)               (((size) + (align)-1) & ~((align)-1))
#define ROT_ALIGN_DOWN(size, align)             ((size) & ~((align)-1))

#define ROT_ATTR_ALIGNED(byte)          __attribute__((aligned(byte)))
#define ROT_ATTR_SECTION(sec_name)      __attribute__((section(#sec_name)))
#define ROT_ATTR_PACKED                 __attribute__((packed))
#define ROT_ATTR_WEAK                   __attribute__((weak))
#define ROT_ATTR_DEPRECATED(mess)       __attribute__ ((deprecated(mess)))
#define ROT_ATTR_UNUSED                 __attribute__ ((unused))
#define ROT_ATTR_USED                   __attribute__ ((used))


#endif
