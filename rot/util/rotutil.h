#ifndef ROTUTIL_H
#define ROTUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#define ROT_DIV_CEIL(size, align)               (((size) + (align) - 1) / (align))
#define ROT_DIV_ROUND_NEAREST(size, align)      (((size) + (align)/2) / (align) ) /* round to nearest integer */
#define ROT_ALIGN_UP(size, align)               (((size) + (align)-1) & ~((align)-1))
#define ROT_ALIGN_DOWN(size, align)             ((size) & ~((align)-1))

#define ROT_ATTR_ALIGNED(byte)          __attribute__((aligned(byte)))
#define ROT_ATTR_SECTION(sec_name)      __attribute__((section(#sec_name)))
#define ROT_ATTR_PACKED                 __attribute__((packed))
#define ROT_ATTR_WEAK                   __attribute__((weak))
#define ROT_ATTR_DEPRECATED(mess)       __attribute__ ((deprecated(mess)))
#define ROT_ATTR_UNUSED                 __attribute__ ((unused))
#define ROT_ATTR_USED                   __attribute__ ((used))

#define rotutil_bit_wrt(val, bit)           ((val) = (bit))
#define rotutil_bit_get(val, bit)           ((val) & (bit))
#define rotutil_bit_set(val, bit)           ((val) |= (bit)) 
#define rotutil_bit_not(val, bit)           ((val) ^= (bit))
#define rotutil_bit_clr(val, bit)           ((val) &= ~(bit))
#define rotutil_bit_mod(val, cbit, sbit)    ((val) = (((val) & (~(cbit))) | (sbit)))


static inline uint16_t rotutil_get_le16(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}
static inline void rotutil_put_le16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}
static inline uint32_t rotutil_get_le32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}
static inline void rotutil_put_le32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}


#ifdef __cplusplus
}
#endif

#endif
