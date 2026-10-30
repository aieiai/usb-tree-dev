#ifndef FDB_CONFIG_H
#define FDB_CONFIG_H


#define FDB_BASE_ADDR               (0x80000000)
#define FDB_TOTAL_SIZ               (0x100000) /* 1MB */
#define FDB_PAGE_SIZ                (0x1000)   /* 4KB */
#define FDB_INVALID_DAT             (0xFFFFFFFF)


// Firmware upgrade area
#define FDB_APP_IMG_NUM             (2)
#define FDB_APP_IMG_SIZ             (FDB_PAGE_SIZ * 64) /* 256KB */
#define FDB_APP_IMG_VERIFY_SIZ      (FDB_PAGE_SIZ) /* 4KB */

#define FDB_APP1_IMG_VERIFY_ADDR    (FDB_BASE_ADDR)
#define FDB_APP1_IMG_ADDR           (FDB_APP1_IMG_VERIFY_ADDR + FDB_APP_IMG_VERIFY_SIZ)

#define FDB_APP2_IMG_VERIFY_ADDR    (FDB_APP1_IMG_ADDR + FDB_APP_IMG_SIZ)
#define FDB_APP2_IMG_ADDR           (FDB_APP2_IMG_VERIFY_ADDR + FDB_APP_IMG_VERIFY_SIZ)

#define FDB_RSV0_ADDR               (FDB_APP2_IMG_ADDR + FDB_APP_IMG_SIZ)
#define FDB_RSV0_SIZ                (FDB_PAGE_SIZ * 2)



// Public parameter configuration area
#define FDB_FACTORY_ADDR            (FDB_RSV0_ADDR + FDB_RSV0_SIZ)
#define FDB_FACTORY_SIZ             (FDB_PAGE_SIZ)

#define FDB_CALIB_ADDR              (FDB_FACTORY_ADDR + FDB_FACTORY_SIZ)
#define FDB_CALIB_SIZ               (FDB_PAGE_SIZ)

#define FDB_COMMON_ADDR             (FDB_CALIB_ADDR + FDB_CALIB_SIZ)
#define FDB_COMMON_SIZ              (FDB_PAGE_SIZ)

#define FDB_RSV1_ADDR               (FDB_COMMON_ADDR + FDB_COMMON_SIZ)
#define FDB_RSV1_SIZ                (FDB_PAGE_SIZ)



// Multiple on-board parameter configurations area

#define FDB_BIF_ADDR                (FDB_RSV1_ADDR + FDB_RSV1_SIZ)
#define FDB_BIN_SIZ                 (FDB_PAGE_SIZ)

#define FDB_MAP_ADDR                (FDB_BIF_ADDR + FDB_BIN_SIZ)
#define FDB_MAP_SIZ                 (FDB_PAGE_SIZ)

#define FDB_ADV_ADDR                (FDB_MAP_ADDR + FDB_MAP_SIZ)
#define FDB_ADV_SIZ                 (FDB_PAGE_SIZ)

#define FDB_RGB_ADDR                (FDB_ADV_ADDR + FDB_ADV_SIZ)
#define FDB_RGB_SIZ                 (FDB_PAGE_SIZ)

#define FDB_RSV2_ADDR               (FDB_RGB_ADDR + FDB_RGB_SIZ)
#define FDB_RSV2_SIZ                (FDB_PAGE_SIZ)

#define FDB_MACRO_ADDR              (FDB_RSV2_ADDR + FDB_RSV2_SIZ)
#define FDB_MACRO_SIZ               (FDB_PAGE_SIZ * 2)

#define FDB_OBCFG_NUM               (4)
#define FDB_OBCFG_ADDR              (FDB_BIF_ADDR)
#define FDB_OBCFG_SIZ               (FDB_BIN_SIZ + FDB_MAP_SIZ + FDB_ADV_SIZ + FDB_RGB_SIZ + FDB_RSV2_SIZ + FDB_MACRO_SIZ)

// Cache
#define FDB_OBCFG_CACHE_ADDR        (FDB_OBCFG_ADDR + (FDB_OBCFG_SIZ * FDB_OBCFG_NUM))
#define FDB_OBCFG_CACHE_SIZ         (FDB_OBCFG_SIZ)


// Flash data base end addr
#define FDB_END_ADDR                (FDB_OBCFG_CACHE_ADDR + FDB_OBCFG_CACHE_SIZ)

_Static_assert(                     FDB_END_ADDR <= (FDB_BASE_ADDR + FDB_TOTAL_SIZ), "");





#endif
