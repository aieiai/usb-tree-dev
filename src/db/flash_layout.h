#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H


/**
 * @brief OTP section image layout
 *      One map per supported SoC. Add a new #elif branch when another chip needs
 *      a different layout, so a missing layout fails the build instead of silently
 *      using the wrong addresses.
 */
#if defined(CHIP_SERIES_HPM5100)
#   define OTP_SEC_IMG_UNIT         (1024u * 16u)   /* 16KB */
#   define OTP_SEC_IMG_OFFSET       (16u)           /* OTP word25[15:0] */
#   define OTP_SEC_MAX_IMG_LEN      (0u)            /* OTP word25[31:16]，0: default use SEC_IMG_OFFSET */
#   define FLASH_TOTAL_SIZ          (0x100000)      /* 1MB */
#   define FLASH_SECTOR_SIZ         (0x1000)        /* 4KB */
#elif defined(CHIP_SERIES_HPM5300)
#   define OTP_SEC_IMG_UNIT         (1024u * 256u)  /* 256KB */
#   define OTP_SEC_IMG_OFFSET       (1u)            /* OTP word25[15:0] */
#   define OTP_SEC_MAX_IMG_LEN      (0u)            /* OTP word25[31:16]，0: default use SEC_IMG_OFFSET */
#   define FLASH_TOTAL_SIZ          (0x100000)      /* 1MB */
#   define FLASH_SECTOR_SIZ         (0x1000)        /* 4KB */
#else
#   error "flash_layout.h: no flash layout defined for this SoC"
#endif

// Flash base message
#define FLASH_BASE_ADDR             (0x80000000)
#define FLASH_APP_IMG_NUM           (2)
#define FLASH_INVALID_DAT           (0xFFFFFFFF)
#define FLASH_APP_IMG_SIZE          (OTP_SEC_IMG_OFFSET * OTP_SEC_IMG_UNIT)

// Firmware upgrade area
// APP1 image, layout follows the official bootrom example.
#define FLASH_APP1_IMG_ADDR         (FLASH_BASE_ADDR)
#define FLASH_APP1_IMG_END_ADDR     (FLASH_APP1_IMG_ADDR + FLASH_APP_IMG_SIZE)

#define FLASH_APP1_EXIP_INFO_ADDR   (FLASH_APP1_IMG_ADDR)
#define FLASH_APP1_EXIP_INFO_SIZE   (0x400)

#define FLASH_APP1_NOR_CFG_ADDR     (FLASH_APP1_EXIP_INFO_ADDR + FLASH_APP1_EXIP_INFO_SIZE)
#define FLASH_APP1_NOR_CFG_SIZE     (0xc00)

#define FLASH_APP1_BOOT_HEAD_ADDR   (FLASH_APP1_NOR_CFG_ADDR + FLASH_APP1_NOR_CFG_SIZE)
#define FLASH_APP1_BOOT_HEAD_SIZE   (0x2000)

#define FLASH_APP1_USER_IMG_ADDR    (FLASH_APP1_BOOT_HEAD_ADDR + FLASH_APP1_BOOT_HEAD_SIZE)
#define FLASH_APP1_USER_IMG_SIZE    (FLASH_APP_IMG_SIZE - (FLASH_APP1_EXIP_INFO_SIZE + FLASH_APP1_NOR_CFG_SIZE + FLASH_APP1_BOOT_HEAD_SIZE))

// APP2 image.
#define FLASH_APP2_IMG_ADDR         (FLASH_APP1_IMG_END_ADDR)
#define FLASH_APP2_IMG_END_ADDR     (FLASH_APP2_IMG_ADDR + FLASH_APP_IMG_SIZE)

#define FLASH_APP2_EXIP_INFO_ADDR   (FLASH_APP2_IMG_ADDR)
#define FLASH_APP2_EXIP_INFO_SIZE   (0x400)

#define FLASH_APP2_NOR_CFG_ADDR     (FLASH_APP2_EXIP_INFO_ADDR + FLASH_APP2_EXIP_INFO_SIZE)
#define FLASH_APP2_NOR_CFG_SIZE     (0xc00)

#define FLASH_APP2_BOOT_HEAD_ADDR   (FLASH_APP2_NOR_CFG_ADDR + FLASH_APP2_NOR_CFG_SIZE)
#define FLASH_APP2_BOOT_HEAD_SIZE   (0x2000)

#define FLASH_APP2_USER_IMG_ADDR    (FLASH_APP2_BOOT_HEAD_ADDR + FLASH_APP2_BOOT_HEAD_SIZE)
#define FLASH_APP2_USER_IMG_SIZE    (FLASH_APP_IMG_SIZE - (FLASH_APP2_EXIP_INFO_SIZE + FLASH_APP2_NOR_CFG_SIZE + FLASH_APP2_BOOT_HEAD_SIZE))

// Official image information pages.
#define FLASH_APP1_IMGINFO_ADDR     (FLASH_APP2_IMG_END_ADDR)
#define FLASH_APP1_IMGINFO_SIZE     (FLASH_SECTOR_SIZ)
#define FLASH_APP2_IMGINFO_ADDR     (FLASH_APP1_IMGINFO_ADDR + FLASH_SECTOR_SIZ)
#define FLASH_APP2_IMGINFO_SIZE     (FLASH_SECTOR_SIZ)

#define FLASH_RSV0_ADDR             (FLASH_APP2_IMGINFO_ADDR + FLASH_SECTOR_SIZ)
#define FLASH_RSV0_SIZ              (FLASH_SECTOR_SIZ * 2)


// Public parameter configuration area
#define FLASH_DATA_BASE_ADDR        (FLASH_RSV0_ADDR + FLASH_RSV0_SIZ)

#define FLASH_FACTORY_ADDR          (FLASH_DATA_BASE_ADDR)
#define FLASH_FACTORY_SIZ           (FLASH_SECTOR_SIZ)

#define FLASH_CALIB_ADDR            (FLASH_FACTORY_ADDR + FLASH_FACTORY_SIZ)
#define FLASH_CALIB_SIZ             (FLASH_SECTOR_SIZ)

#define FLASH_COMMON_ADDR           (FLASH_CALIB_ADDR + FLASH_CALIB_SIZ)
#define FLASH_COMMON_SIZ            (FLASH_SECTOR_SIZ)

#define FLASH_RSV1_ADDR             (FLASH_COMMON_ADDR + FLASH_COMMON_SIZ)
#define FLASH_RSV1_SIZ              (FLASH_SECTOR_SIZ)


// Multiple on-board parameter configurations area
#define FLASH_OBI_ADDR             (FLASH_RSV1_ADDR + FLASH_RSV1_SIZ)
#define FLASH_OBI_SIZ              (FLASH_SECTOR_SIZ)

#define FLASH_MAP_ADDR              (FLASH_OBI_ADDR + FLASH_OBI_SIZ)
#define FLASH_MAP_SIZ               (FLASH_SECTOR_SIZ)

#define FLASH_ATTR_ADDR             (FLASH_MAP_ADDR + FLASH_MAP_SIZ)
#define FLASH_ATTR_SIZ              (FLASH_SECTOR_SIZ)

#define FLASH_ADV_ADDR              (FLASH_ATTR_ADDR + FLASH_ATTR_SIZ)
#define FLASH_ADV_SIZ               (FLASH_SECTOR_SIZ)

#define FLASH_RGB_ADDR              (FLASH_ADV_ADDR + FLASH_ADV_SIZ)
#define FLASH_RGB_SIZ               (FLASH_SECTOR_SIZ)

#define FLASH_RSV2_ADDR             (FLASH_RGB_ADDR + FLASH_RGB_SIZ)
#define FLASH_RSV2_SIZ              (FLASH_SECTOR_SIZ)

#define FLASH_MACRO_ADDR            (FLASH_RSV2_ADDR + FLASH_RSV2_SIZ)
#define FLASH_MACRO_SIZ             (FLASH_SECTOR_SIZ * 2)

#define FLASH_OBCFG_NUM             (4)
#define FLASH_OBCFG_ADDR            (FLASH_OBI_ADDR)
#define FLASH_OBCFG_SIZ             (FLASH_OBI_SIZ + FLASH_MAP_SIZ + FLASH_ATTR_SIZ + FLASH_ADV_SIZ + FLASH_RGB_SIZ + FLASH_RSV2_SIZ + FLASH_MACRO_SIZ)
// Cache
#define FLASH_OBCFG_CACHE_ADDR      (FLASH_OBCFG_ADDR + (FLASH_OBCFG_SIZ * FLASH_OBCFG_NUM))
#define FLASH_OBCFG_CACHE_SIZ       (FLASH_OBCFG_SIZ)


// Flash data base end addr
#define FLASH_DATA_END_ADDR         (FLASH_OBCFG_CACHE_ADDR + FLASH_OBCFG_CACHE_SIZ)

_Static_assert(FLASH_DATA_END_ADDR <= (FLASH_BASE_ADDR + FLASH_TOTAL_SIZ), "DB_DATA_END_ADDR must be less than FLASH_BASE_ADDR + FLASH_TOTAL_SIZ");





#endif
