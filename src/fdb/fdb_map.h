#ifndef FDB_MAP_H
#define FDB_MAP_H

// Flash base message
#define FDB_BASE_ADDR               (0x80000000)
#define FDB_TOTAL_SIZ               (0x100000) /* 1MB */
#define FDB_SECTOR_SIZ              (0x1000)   /* 4KB */
#define FDB_INVALID_DAT             (0xFFFFFFFF)


// Firmware upgrade area
#define FDB_APP_IMG_NUM             (2)
#define FDB_APP_IMG_SIZ             (1024 * 256) /* 256KB */
#define FDB_APP_IMGINFO_SIZ         (FDB_SECTOR_SIZ) /* 4KB */

// APP1 image, layout follows the official bootrom example.
#define FDB_APP1_IMG_ADDR           (FDB_BASE_ADDR)
#define FDB_APP1_IMG_END_ADDR       (FDB_APP1_IMG_ADDR + FDB_APP_IMG_SIZ)

#define FDB_APP1_EXIP_INFO_ADDR     (FDB_APP1_IMG_ADDR)
#define FDB_APP1_EXIP_INFO_SIZE     (0x400)

#define FDB_APP1_NOR_CFG_ADDR       (FDB_APP1_EXIP_INFO_ADDR + FDB_APP1_EXIP_INFO_SIZE)
#define FDB_APP1_NOR_CFG_SIZE       (0xc00)

#define FDB_APP1_BOOT_HEAD_ADDR     (FDB_APP1_NOR_CFG_ADDR + FDB_APP1_NOR_CFG_SIZE)
#define FDB_APP1_BOOT_HEAD_SIZE     (0x2000)

#define FDB_APP1_USER_IMG_ADDR      (FDB_APP1_BOOT_HEAD_ADDR + FDB_APP1_BOOT_HEAD_SIZE)
#define FDB_APP1_USER_IMG_SIZE      (FDB_APP_IMG_SIZ - (FDB_APP1_EXIP_INFO_SIZE + FDB_APP1_NOR_CFG_SIZE + FDB_APP1_BOOT_HEAD_SIZE))

// APP2 image.
#define FDB_APP2_IMG_ADDR           (FDB_APP1_IMG_END_ADDR)
#define FDB_APP2_IMG_END_ADDR       (FDB_APP2_IMG_ADDR + FDB_APP_IMG_SIZ)

#define FDB_APP2_EXIP_INFO_ADDR     (FDB_APP2_IMG_ADDR)
#define FDB_APP2_EXIP_INFO_SIZE     (0x400)

#define FDB_APP2_NOR_CFG_ADDR       (FDB_APP2_EXIP_INFO_ADDR + FDB_APP2_EXIP_INFO_SIZE)
#define FDB_APP2_NOR_CFG_SIZE       (0xc00)

#define FDB_APP2_BOOT_HEAD_ADDR     (FDB_APP2_NOR_CFG_ADDR + FDB_APP2_NOR_CFG_SIZE)
#define FDB_APP2_BOOT_HEAD_SIZE     (0x2000)

#define FDB_APP2_USER_IMG_ADDR      (FDB_APP2_BOOT_HEAD_ADDR + FDB_APP2_BOOT_HEAD_SIZE)
#define FDB_APP2_USER_IMG_SIZE      (FDB_APP_IMG_SIZ - (FDB_APP2_EXIP_INFO_SIZE + FDB_APP2_NOR_CFG_SIZE + FDB_APP2_BOOT_HEAD_SIZE))

// Official image information pages.
#define FDB_APP1_IMGINFO_ADDR       (FDB_APP2_IMG_END_ADDR)
#define FDB_APP1_IMGINFO_SIZE       (FDB_APP_IMGINFO_SIZ)
#define FDB_APP2_IMGINFO_ADDR       (FDB_APP1_IMGINFO_ADDR + FDB_APP_IMGINFO_SIZ)
#define FDB_APP2_IMGINFO_SIZE       (FDB_APP_IMGINFO_SIZ)

#define FDB_RSV0_ADDR               (FDB_APP2_IMGINFO_ADDR + FDB_APP_IMGINFO_SIZ)
#define FDB_RSV0_SIZ                (FDB_SECTOR_SIZ * 2)


// Public parameter configuration area
#define FDB_DATA_BASE_ADDR          (FDB_RSV0_ADDR + FDB_RSV0_SIZ)

#define FDB_FACTORY_ADDR            (FDB_DATA_BASE_ADDR)
#define FDB_FACTORY_SIZ             (FDB_SECTOR_SIZ)

#define FDB_CALIB_ADDR              (FDB_FACTORY_ADDR + FDB_FACTORY_SIZ)
#define FDB_CALIB_SIZ               (FDB_SECTOR_SIZ)

#define FDB_COMMON_ADDR             (FDB_CALIB_ADDR + FDB_CALIB_SIZ)
#define FDB_COMMON_SIZ              (FDB_SECTOR_SIZ)

#define FDB_RSV1_ADDR               (FDB_COMMON_ADDR + FDB_COMMON_SIZ)
#define FDB_RSV1_SIZ                (FDB_SECTOR_SIZ)


// Multiple on-board parameter configurations area
#define FDB_SOBI_ADDR               (FDB_RSV1_ADDR + FDB_RSV1_SIZ)
#define FDB_SOBI_SIZ                (FDB_SECTOR_SIZ)

#define FDB_MAP_ADDR                (FDB_SOBI_ADDR + FDB_SOBI_SIZ)
#define FDB_MAP_SIZ                 (FDB_SECTOR_SIZ)

#define FDB_ADV_ADDR                (FDB_MAP_ADDR + FDB_MAP_SIZ)
#define FDB_ADV_SIZ                 (FDB_SECTOR_SIZ)

#define FDB_RGB_ADDR                (FDB_ADV_ADDR + FDB_ADV_SIZ)
#define FDB_RGB_SIZ                 (FDB_SECTOR_SIZ)

#define FDB_RSV2_ADDR               (FDB_RGB_ADDR + FDB_RGB_SIZ)
#define FDB_RSV2_SIZ                (FDB_SECTOR_SIZ)

#define FDB_MACRO_ADDR              (FDB_RSV2_ADDR + FDB_RSV2_SIZ)
#define FDB_MACRO_SIZ               (FDB_SECTOR_SIZ * 2)

#define FDB_OBCFG_NUM               (4)
#define FDB_OBCFG_ADDR              (FDB_SOBI_ADDR)
#define FDB_OBCFG_SIZ               (FDB_SOBI_SIZ + FDB_MAP_SIZ + FDB_ADV_SIZ + FDB_RGB_SIZ + FDB_RSV2_SIZ + FDB_MACRO_SIZ)
// Cache
#define FDB_OBCFG_CACHE_ADDR        (FDB_OBCFG_ADDR + (FDB_OBCFG_SIZ * FDB_OBCFG_NUM))
#define FDB_OBCFG_CACHE_SIZ         (FDB_OBCFG_SIZ)


// Flash data base end addr
#define FDB_DATA_END_ADDR           (FDB_OBCFG_CACHE_ADDR + FDB_OBCFG_CACHE_SIZ)

_Static_assert(                     FDB_DATA_END_ADDR <= (FDB_BASE_ADDR + FDB_TOTAL_SIZ), "FDB_DATA_END_ADDR must be less than FDB_BASE_ADDR + FDB_TOTAL_SIZ");





#endif
