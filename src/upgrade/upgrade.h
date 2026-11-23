#ifndef UPGRADE_H
#define UPGRADE_H

#include <stdbool.h>
#include <stdint.h>

#include "flash_layout.h"
#include "rotsta.h"


/*
 * Layout inside one application slot (relative to the slot base):
 *   +0x0000  EXIP container   (encrypted images only)
 *   +0x0400  NOR configuration
 *   +0x1000  HPM boot header
 *   +0x3000  user application
 */
#define UPGRADE_IMAGE_BASE_PLAIN    (FLASH_APP1_NOR_CFG_ADDR - FLASH_APP1_IMG_ADDR)
#define UPGRADE_BOOT_HEAD_OFFSET    (FLASH_APP1_BOOT_HEAD_ADDR - FLASH_APP1_IMG_ADDR)
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


typedef struct __attribute__((packed)) {
    uint8_t  slot_act;          /* currently booted slot */
    uint8_t  slot_tag;          /* slot the next upgrade will write */
    uint8_t  slot_cnt;
    uint8_t  encrypted;         /* OTP EXIP enabled */
    uint8_t  remap_enabled;     /* ROM remap, i.e. running APP2 */
    uint8_t  boot_ota_enabled;  /* OTP SEC_IMG_OFFSET is programmed */
    uint8_t  otp_address_match; /* OTP SEC_IMG_OFFSET points at APP2 */
    uint8_t  otp_length_match;  /* OTP image length covers one slot */
    uint8_t  slot_valid[FLASH_APP_IMG_NUM];
    uint8_t  slot_retired[FLASH_APP_IMG_NUM];
    uint16_t slot_version[FLASH_APP_IMG_NUM];
    uint32_t slot_size;
    uint32_t image_base_offset;
    uint32_t file_boot_offset;
    uint32_t otp_offset;        /* OTP SEC_IMG_OFFSET in bytes */
    uint32_t otp_size;          /* OTP MAX_IMG_LEN in bytes, 0 means default */
    uint16_t otp_sec_img_offset;/* raw OTP word25[15:0], unit 16KB */
    uint16_t otp_max_img_len;   /* raw OTP word25[31:16], 0 means default */
    uint64_t firmware_version;  /* running FW_VER */
} upgrade_info_t;

_Static_assert(FLASH_APP_IMG_NUM == 2U, "upgrade info layout assumes two slots");
_Static_assert(sizeof(upgrade_info_t) == 48U, "upgrade info payload layout changed");

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
void upgrade_restart(void);


#endif
