#ifndef UPGRADE_H
#define UPGRADE_H

#include <stdbool.h>
#include <stdint.h>

#include "fdb_map.h"
#include "rotsta.h"


/*
 * Layout inside one application slot (relative to the slot base):
 *   +0x0000  EXIP container   (encrypted images only)
 *   +0x0400  NOR configuration
 *   +0x1000  HPM boot header
 *   +0x3000  user application
 */
#define UPGRADE_IMAGE_BASE_PLAIN    (FDB_APP1_NOR_CFG_ADDR - FDB_APP1_IMG_ADDR)
#define UPGRADE_BOOT_HEAD_OFFSET    (FDB_APP1_BOOT_HEAD_ADDR - FDB_APP1_IMG_ADDR)
#define UPGRADE_BOOT_HEAD_SIZE      (16U)
#define UPGRADE_BOOT_HEAD_TAG       (0xbfU)
#define UPGRADE_BOOT_HEAD_VERSION   (0x10U)


enum {
    rotsta_upgrade_init_fail = ROTSTA_MAKE(rotsta_group_upgrade, 1),
    rotsta_upgrade_not_init,
    rotsta_upgrade_busy,
    rotsta_upgrade_invalid_slot,
    rotsta_upgrade_invalid_header,
    rotsta_upgrade_image_too_large,
    rotsta_upgrade_incomplete,
    rotsta_upgrade_checksum_mismatch,
    rotsta_upgrade_invalid_image,
    /* Kept so the numeric codes below stay stable for the Rot-CC host. */
    rotsta_upgrade_record_invalid,
    rotsta_upgrade_verify_fail,
    rotsta_upgrade_boot_layout_mismatch,
    rotsta_upgrade_exip_image_required,
};


typedef struct {
    uint8_t active_slot;
    uint8_t target_slot;
    uint8_t encrypted;
    uint8_t slot_count;
    uint16_t active_version;
    uint32_t slot_size;
    uint32_t image_base_offset;
} upgrade_info_t;


/* Initializes the shared flash HAL and detects the currently booted slot. */
rotsta_t upgrade_init(void);

/* Current slot, target slot, slot size and EXIP state for the Rot-CC INFO reply. */
rotsta_t upgrade_get_info(upgrade_info_t *info);

/*
 * Start a transactional upgrade. The target slot is erased here, the running
 * slot stays untouched. image_crc32 is the standard CRC-32 of the complete
 * image that will be streamed through upgrade_write().
 */
rotsta_t upgrade_start(uint32_t image_size, uint32_t image_crc32);

/* Streaming write, len may be unaligned and must not exceed the image size. */
rotsta_t upgrade_write(const void *data, uint32_t len);

/*
 * Verify the stored image, publish the boot header and retire the running slot
 * when the new version is lower.
 */
rotsta_t upgrade_finish(void);

/* Abort the session and erase the target slot. */
rotsta_t upgrade_abort(void);

/* Reset after a successful upgrade. */
void upgrade_reboot(void);


#endif
