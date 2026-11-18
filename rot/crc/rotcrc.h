#ifndef ROTCRC_H
#define ROTCRC_H

#include <stdint.h>

static inline uint8_t rotcrc8_update(uint8_t crc, const uint8_t *data, uint32_t len)
{
    uint8_t poly = 0x07U;
    for (uint32_t i = 0U; i < len; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ poly) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}


static inline uint16_t rotcrc16_update(uint16_t crc, const uint8_t *data, uint32_t len)
{
    uint16_t poly = 0x1021U;
    for (uint32_t i = 0U; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ poly) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static inline uint32_t rotcrc32_update(uint32_t crc, const uint8_t *data, uint32_t size)
{
    uint32_t poly = 0xedb88320UL;
    for (uint32_t i = 0U; i < size; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc >> 1) ^ (poly & (0U - (crc & 1U)));
        }
    }
    return crc;
}

#endif
