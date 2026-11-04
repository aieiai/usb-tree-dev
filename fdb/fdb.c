#include "fdb.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "fdb_map.h"
#include "hal_flash.h"
#include "hal_osal.h"
#include "tx_config.h"

#define TAG "fdb: "

#define FDB_MAGIC                 (0x46444231UL)
#define FDB_VERSION               (1U)
#define FDB_SAVE_DELAY_MS         (5000U)
#define FDB_IMMEDIATE_TIMEOUT_MS  (5000U)
#define FDB_TYPE_BIT(type)        (1UL << (uint32_t)(type))

_Static_assert(FDB_TYPE_COUNT <= 32U, "FDB dirty mask supports at most 32 types");

typedef struct {
    uint32_t    offset;
    uint32_t    size;
    const char *name;
    uint32_t    save_delay_ms;
} fdb_type_info_t;

typedef union {
    fdb_fac_t   fac;
    fdb_calib_t calib;
    fdb_common_t common;
    fdb_bif_t   bif;
    fdb_map_t   map;
    fdb_adv_t   adv;
    fdb_rgb_t   rgb;
    fdb_rsv2_t  rsv2;
    fdb_macro_t macro;
} fdb_record_t;

_Static_assert(sizeof(fdb_record_t) <= FDB_PAGE_SIZ, "FDB record must fit in one Flash sector");

static const fdb_type_info_t s_type_info[FDB_TYPE_COUNT] = {
    [FDB_TYPE_FACTORY] = {
        .offset = FDB_FACTORY_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_fac_t),
        .name = "fdb_factory",
        .save_delay_ms = 0U,
    },
    [FDB_TYPE_CALIB] = {
        .offset = FDB_CALIB_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_calib_t),
        .name = "fdb_calib",
        .save_delay_ms = 0U,
    },
    [FDB_TYPE_COMMON] = {
        .offset = FDB_COMMON_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_common_t),
        .name = "fdb_common",
        .save_delay_ms = FDB_SAVE_DELAY_MS,
    },
    [FDB_TYPE_BIF] = {
        .offset = FDB_BIF_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_bif_t),
        .name = "fdb_bif",
        .save_delay_ms = 0U,
    },
    [FDB_TYPE_MAP] = {
        .offset = FDB_MAP_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_map_t),
        .name = "fdb_map",
        .save_delay_ms = FDB_SAVE_DELAY_MS,
    },
    [FDB_TYPE_ADV] = {
        .offset = FDB_ADV_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_adv_t),
        .name = "fdb_adv",
        .save_delay_ms = FDB_SAVE_DELAY_MS,
    },
    [FDB_TYPE_RGB] = {
        .offset = FDB_RGB_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_rgb_t),
        .name = "fdb_rgb",
        .save_delay_ms = FDB_SAVE_DELAY_MS,
    },
    [FDB_TYPE_RSV2] = {
        .offset = FDB_RSV2_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_rsv2_t),
        .name = "fdb_rsv2",
        .save_delay_ms = FDB_SAVE_DELAY_MS,
    },
    [FDB_TYPE_MACRO] = {
        .offset = FDB_MACRO_ADDR - FDB_BASE_ADDR,
        .size = sizeof(fdb_macro_t),
        .name = "fdb_macro",
        .save_delay_ms = FDB_SAVE_DELAY_MS,
    },
};

fdb_data_t fdb_data;

static hal_osal_thread_t s_thread;
static uint8_t s_thread_stack[FDB_TASK_STACK] __attribute__((aligned(8U)));

static hal_osal_mutex_t s_api_mutex;
static hal_osal_mutex_t s_data_mutex;
static hal_osal_sem_t s_wake_sem;
static hal_osal_sem_t s_done_sem;

static fdb_record_t s_pending[FDB_TYPE_COUNT];
static fdb_record_t s_save_record;
static uint32_t s_generation[FDB_TYPE_COUNT];
static rot_state_t s_save_result[FDB_TYPE_COUNT];
static volatile uint32_t s_dirty_mask;
static bool s_initialized;

static bool fdb_type_valid(fdb_type_t type)
{
    return (uint32_t)type < (uint32_t)FDB_TYPE_COUNT;
}

static void *fdb_global_record(fdb_type_t type)
{
    switch (type) {
    case FDB_TYPE_FACTORY:
        return &fdb_data.fac;
    case FDB_TYPE_CALIB:
        return &fdb_data.calib;
    case FDB_TYPE_COMMON:
        return &fdb_data.common;
    case FDB_TYPE_BIF:
        return &fdb_data.bif;
    case FDB_TYPE_MAP:
        return &fdb_data.map;
    case FDB_TYPE_ADV:
        return &fdb_data.adv;
    case FDB_TYPE_RGB:
        return &fdb_data.rgb;
    case FDB_TYPE_RSV2:
        return &fdb_data.rsv2;
    case FDB_TYPE_MACRO:
        return &fdb_data.macro;
    default:
        return NULL;
    }
}

static void fdb_stamp_header(fdb_head_t *header, fdb_type_t type)
{
    const char *name = s_type_info[type].name;
    size_t name_len = strlen(name);

    header->magic = FDB_MAGIC;
    header->version = FDB_VERSION;
    memset(header->name, 0, sizeof(header->name));
    if (name_len > sizeof(header->name)) {
        name_len = sizeof(header->name);
    }
    memcpy(header->name, name, name_len);
}

static bool fdb_header_valid(const fdb_head_t *header, fdb_type_t type)
{
    const char *name = s_type_info[type].name;
    size_t name_len = strlen(name);

    if ((header->magic != FDB_MAGIC) || (header->version != FDB_VERSION)) {
        return false;
    }
    if (name_len > sizeof(header->name)) {
        return false;
    }
    if (memcmp(header->name, name, name_len) != 0) {
        return false;
    }
    for (size_t i = name_len; i < sizeof(header->name); i++) {
        if (header->name[i] != 0U) {
            return false;
        }
    }
    return true;
}

static uint32_t fdb_dirty_snapshot(void)
{
    uint32_t dirty;

    if (!hal_osal_mutex_lock(&s_data_mutex)) {
        return 0U;
    }
    dirty = s_dirty_mask;
    (void)hal_osal_mutex_unlock(&s_data_mutex);
    return dirty;
}

static bool fdb_type_is_dirty(fdb_type_t type)
{
    bool dirty;

    if (!hal_osal_mutex_lock(&s_data_mutex)) {
        return false;
    }
    dirty = (s_dirty_mask & FDB_TYPE_BIT(type)) != 0U;
    (void)hal_osal_mutex_unlock(&s_data_mutex);
    return dirty;
}

static rot_state_t fdb_snapshot_type(fdb_type_t type)
{
    void *source = fdb_global_record(type);

    if (source == NULL) {
        return rot_state_fdb_invalid_type;
    }
    if (!hal_osal_mutex_lock(&s_data_mutex)) {
        return rot_state_fdb_lock_fail;
    }

    fdb_stamp_header((fdb_head_t *)source, type);
    memcpy(&s_pending[type], source, s_type_info[type].size);
    s_generation[type]++;
    s_dirty_mask |= FDB_TYPE_BIT(type);

    (void)hal_osal_mutex_unlock(&s_data_mutex);
    return rot_state_success;
}

static rot_state_t fdb_save_type(fdb_type_t type)
{
    const fdb_type_info_t *info = &s_type_info[type];
    uint32_t type_bit = FDB_TYPE_BIT(type);
    uint32_t generation;
    uint32_t write_size = (info->size + 3U) & ~3U;
    rot_state_t result;

    if ((write_size == 0U) || (write_size > FDB_PAGE_SIZ)) {
        return rot_state_fdb_save_fail;
    }
    if (!hal_osal_mutex_lock(&s_data_mutex)) {
        return rot_state_fdb_lock_fail;
    }
    memcpy(&s_save_record, &s_pending[type], info->size);
    generation = s_generation[type];
    (void)hal_osal_mutex_unlock(&s_data_mutex);

    result = hal_flash_erase(info->offset, FDB_PAGE_SIZ);
    if (result == rot_state_success) {
        result = hal_flash_write(info->offset, &s_save_record, write_size);
    }

    if (!hal_osal_mutex_lock(&s_data_mutex)) {
        return rot_state_fdb_lock_fail;
    }
    if (s_generation[type] == generation) {
        s_dirty_mask &= ~type_bit;
    }
    s_save_result[type] = result;
    (void)hal_osal_mutex_unlock(&s_data_mutex);

    if (result != rot_state_success) {
        printf(TAG "save %s failed: 0x%08x\r\n", info->name, (unsigned)result);
    }
    return result;
}

static void fdb_thread_entry(void *argument)
{
    (void)argument;

    for (;;) {
        uint32_t dirty;
        bool saved_immediate = false;

        (void)hal_osal_sem_wait(&s_wake_sem, HAL_OSAL_WAIT_FOREVER);

        for (;;) {
            dirty = fdb_dirty_snapshot();
            if (dirty == 0U) {
                break;
            }

            saved_immediate = false;
            for (uint32_t i = 0U; i < (uint32_t)FDB_TYPE_COUNT; i++) {
                if (((dirty & FDB_TYPE_BIT(i)) != 0U) &&
                    (s_type_info[i].save_delay_ms == 0U)) {
                    (void)fdb_save_type((fdb_type_t)i);
                    saved_immediate = true;
                }
            }
            if (saved_immediate) {
                (void)hal_osal_sem_post(&s_done_sem);
                continue;
            }

            if (hal_osal_sem_wait(&s_wake_sem, FDB_SAVE_DELAY_MS)) {
                continue;
            }

            dirty = fdb_dirty_snapshot();
            for (uint32_t i = 0U; i < (uint32_t)FDB_TYPE_COUNT; i++) {
                if (((dirty & FDB_TYPE_BIT(i)) != 0U) &&
                    (s_type_info[i].save_delay_ms != 0U)) {
                    (void)fdb_save_type((fdb_type_t)i);
                }
            }
            break;
        }
    }
}

static void fdb_load_records(void)
{
    uint32_t loaded_count = 0U;
    uint32_t default_count = 0U;

    s_dirty_mask = 0U;

    for (uint32_t i = 0U; i < (uint32_t)FDB_TYPE_COUNT; i++) {
        fdb_type_t type = (fdb_type_t)i;
        void *record = fdb_global_record(type);
        rot_state_t result;
        bool valid = false;

        s_generation[i] = 0U;
        s_save_result[i] = rot_state_success;

        result = hal_flash_read_mem(s_type_info[i].offset, record, s_type_info[i].size);
        if (result == rot_state_success) {
            valid = fdb_header_valid((fdb_head_t *)record, type);
        } 
        else {
            printf(TAG "read %s failed: 0x%08x\r\n", s_type_info[i].name, (unsigned)result);
        }
        if (!valid) {
            memset(record, 0, s_type_info[i].size);
            fdb_stamp_header((fdb_head_t *)record, type);
            memcpy(&s_pending[i], record, s_type_info[i].size);
            s_dirty_mask |= FDB_TYPE_BIT(i);
            default_count++;
        } 
        else {
            loaded_count++;
        }
    }

    printf(TAG "loaded %u record(s), initialized %u default record(s)\r\n",
           (unsigned)loaded_count, (unsigned)default_count);
}

rot_state_t fdb_init(void)
{
    rot_state_t result;

    if (s_initialized) {
        return rot_state_success;
    }

    result = hal_flash_init();
    if (result != rot_state_success) {
        return rot_state_fdb_init_fail;
    }
    if (!hal_osal_mutex_create(&s_api_mutex, "fdb_api") ||
        !hal_osal_mutex_create(&s_data_mutex, "fdb_data") ||
        !hal_osal_sem_create(&s_wake_sem, "fdb_wake", 0U, 1U) ||
        !hal_osal_sem_create(&s_done_sem, "fdb_done", 0U, 1U)) {
        return rot_state_fdb_init_fail;
    }

    fdb_load_records();
    s_initialized = true;

    if (!hal_osal_thread_create(&s_thread,
                                "thread_fdb",
                                s_thread_stack,
                                sizeof(s_thread_stack),
                                FDB_TASK_PRIORITY,
                                fdb_thread_entry,
                                NULL)) {
        s_initialized = false;
        return rot_state_fdb_init_fail;
    }

    if (s_dirty_mask != 0U) {
        (void)hal_osal_sem_post(&s_wake_sem);
    }
    return rot_state_success;
}

rot_state_t fdb_update(fdb_type_t type)
{
    bool immediate;
    rot_state_t result;

    if (!s_initialized) {
        return rot_state_fdb_not_init;
    }
    if (!fdb_type_valid(type)) {
        return rot_state_fdb_invalid_type;
    }
    if (!hal_osal_mutex_lock(&s_api_mutex)) {
        return rot_state_fdb_lock_fail;
    }

    immediate = s_type_info[type].save_delay_ms == 0U;
    if (immediate) {
        (void)hal_osal_sem_reset(&s_done_sem);
    }

    result = fdb_snapshot_type(type);
    if (result == rot_state_success) {
        (void)hal_osal_sem_post(&s_wake_sem);
    }

    if ((result == rot_state_success) && immediate) {
        while (fdb_type_is_dirty(type)) {
            if (!hal_osal_sem_wait(&s_done_sem, FDB_IMMEDIATE_TIMEOUT_MS)) {
                result = rot_state_timeout;
                break;
            }
        }
        if (result == rot_state_success) {
            if (!hal_osal_mutex_lock(&s_data_mutex)) {
                result = rot_state_fdb_lock_fail;
            } else {
                result = s_save_result[type];
                (void)hal_osal_mutex_unlock(&s_data_mutex);
            }
        }
    }

    (void)hal_osal_mutex_unlock(&s_api_mutex);
    return result;
}
