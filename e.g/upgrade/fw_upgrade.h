#ifndef FW_UPGRADE_H
#define FW_UPGRADE_H

#include <stdbool.h>
#include <stdint.h>

#include "gs_ec.h"



typedef enum {
    UPGRADE_PAYLOAD_START = 0,
    UPGRADE_PAYLOAD_FINISH,
    UPGRADE_PAYLOAD_ABORT,
    UPGRADE_PAYLOAD_INVALID,
} upgrade_payload_action_t;

enum {
    UPGRADE_INFO_ACTIVE_HEADER_VALID = (1u << 0),
    UPGRADE_INFO_TARGET_HEADER_VALID = (1u << 1),
    UPGRADE_INFO_TRANSFER_ACTIVE     = (1u << 2),
    UPGRADE_INFO_OTP_ADDRESS_MATCH   = (1u << 3),
    UPGRADE_INFO_OTP_LENGTH_MATCH    = (1u << 4),
};

typedef struct {
    uint8_t  index;
    uint32_t app_offset;
    uint32_t app_size;
    uint32_t info_offset;
    uint32_t info_size;
} upgrade_slot_t;

typedef struct __attribute__((packed)) {
    uint8_t  active_slot;
    uint8_t  target_slot;
    uint8_t  encrypted;
    uint8_t  flags;
    uint16_t active_sw_version;
    uint16_t target_sw_version;
    uint32_t app_size;
    uint32_t image_base_offset;
    uint32_t file_boot_head_offset;
} upgrade_info_t;

_Static_assert(sizeof(upgrade_info_t) == 20u, "upgrade_info_t must be 20 bytes");

extern const upgrade_slot_t upgrade_slot[];

gs_ec_t upgrade_start(const uint8_t *data, uint16_t len);
gs_ec_t upgrade_finish(const uint8_t *data, uint16_t len);
gs_ec_t upgrade_abort(const uint8_t *data, uint16_t len);
gs_ec_t upgrade_write(const uint8_t *data, uint16_t len, uint32_t *seq);

bool upgrade_is_active(void);
uint32_t upgrade_received(void);
uint32_t upgrade_image_size(void);

#endif
