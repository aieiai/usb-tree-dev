#ifndef UPGRADE_H
#define UPGRADE_H

#include <stdbool.h>
#include <stdint.h>

#include "fdb_map.h"
#include "rot_state.h"


/* Wire format used by the official HPMicro OTA example. */
#define UPGRADE_IMAGE_HEADER_MAGIC  (0xbeaf5aa5UL)
#define UPGRADE_DEVICE_ID           (0x6750UL)
#define UPGRADE_IMAGE_MAX_SIZE      (FDB_APP_IMG_SIZ)

/*
 * FDB_APPx_IMGINFO_ADDR starts with the official 16-byte header. The extended
 * record follows it in the same page.
 */
#define UPGRADE_RECORD_MAGIC        (0x55504752UL) /* "UPGR" */
#define UPGRADE_RECORD_VERSION      (1U)
#define UPGRADE_RECORD_STATE_READY  (0xa5U)


enum {
    rot_state_upgrade_init_fail = ROT_STATE_MAKE(rot_state_group_upgrade, 1),
    rot_state_upgrade_not_init,
    rot_state_upgrade_busy,
    rot_state_upgrade_invalid_slot,
    rot_state_upgrade_invalid_header,
    rot_state_upgrade_image_too_large,
    rot_state_upgrade_incomplete,
    rot_state_upgrade_checksum_mismatch,
    rot_state_upgrade_invalid_image,
    rot_state_upgrade_record_invalid,
    rot_state_upgrade_verify_fail,
    rot_state_upgrade_boot_layout_mismatch,
};


typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t device;
    uint32_t len;
    uint32_t checksum; /* 32-bit byte sum, compatible with the official example. */
} upgrade_image_header_t;


typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t slot;
    uint8_t state;
    uint8_t reserved0[2];
    uint32_t image_size;
    uint32_t image_checksum;
    uint32_t image_crc32;
    uint32_t image_offset;
    uint16_t sw_version;
    uint16_t boot_header_length;
    uint32_t reserved1;
    uint32_t crc32; /* CRC-32 over all preceding fields. */
} upgrade_record_t;


typedef enum {
    UPGRADE_STATE_IDLE = 0,
    UPGRADE_STATE_RECEIVING,
    UPGRADE_STATE_VERIFIED,
    UPGRADE_STATE_FAILED,
} upgrade_state_t;


typedef struct {
    upgrade_state_t state;
    uint8_t active_slot;
    uint8_t target_slot;
    uint32_t image_offset;
    uint32_t image_size;
    uint32_t received_size;
    uint32_t expected_checksum;
    uint32_t checksum;
    rot_state_t last_result;
} upgrade_status_t;


typedef struct {
    uint8_t active_slot;
    uint8_t remap_enabled;
    uint8_t boot_ota_enabled;
    uint8_t boot_layout_matches;
    uint8_t slot_valid[FDB_APP_IMG_NUM];
    uint16_t active_version;
    uint32_t firmware_version;
    uint16_t active_boot_header_length;
    uint16_t slot_version[FDB_APP_IMG_NUM];
    uint16_t slot_boot_header_length[FDB_APP_IMG_NUM];
    uint16_t otp_sec_img_offset;
    uint16_t otp_max_img_len;
    uint16_t configured_slot_size_256k;
} upgrade_runtime_info_t;


_Static_assert(FDB_APP_IMG_NUM == 2, "upgrade module requires two application slots");
_Static_assert(sizeof(upgrade_image_header_t) == 16U, "wire image header must stay 16 bytes");
_Static_assert(sizeof(upgrade_record_t) == 40U, "upgrade record layout changed unexpectedly");


/* Initializes the shared flash HAL and detects the currently booted slot. */
rot_state_t upgrade_init(void);

rot_state_t upgrade_get_status(upgrade_status_t *status);
rot_state_t upgrade_get_slot_info(uint8_t slot, uint32_t *image_offset, uint32_t *capacity);
rot_state_t upgrade_get_active_slot(uint8_t *slot);
rot_state_t upgrade_get_runtime_info(upgrade_runtime_info_t *info);

/*
 * Start a transactional upgrade. The target image area is erased here, while
 * the current slot remains untouched.
 */
rot_state_t upgrade_begin(const upgrade_image_header_t *header);

/* Strict write interface: len may be unaligned, but must not exceed the image. */
rot_state_t upgrade_write(const void *data, uint32_t len);

/*
 * Finish the image, verify its byte-sum and boot header, then publish a CRC
 * protected record in the target slot's verify page.
 */
rot_state_t upgrade_finish(void);

/*
 * Compatibility stream interface for the official packet format:
 * the first call must contain the complete 16-byte image header. Later
 * XMODEM padding after the declared image length is ignored.
 */
rot_state_t upgrade_feed(const void *data, uint32_t len, bool *image_complete);
rot_state_t upgrade_auto_write(const void *data, uint32_t len, bool *image_complete);

/* Abort the current receive session. The inactive slot can be retried safely. */
rot_state_t upgrade_abort(void);

/* Read and validate the persistent record for one slot. */
rot_state_t upgrade_read_record(uint8_t slot, upgrade_record_t *record);

/* Validate both the record and the complete stored image checksum. */
rot_state_t upgrade_validate_slot(uint8_t slot, upgrade_record_t *record);

/* Reset after a successful upgrade. The bootloader should consume the record. */
void upgrade_reboot(void);


#endif
