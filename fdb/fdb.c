#include "fdb.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "rot_utils.h"

#include "fdb_map.h"
#include "hal_flash.h"

#include "tx_api.h"
#include "tx_config.h"

#define TAG "fdb: "

#define FDB_SAVE_DELAY_MS       (5000U)
#define FDB_NOW_TIMEOUT_MS      (5000U)
#define FDB_ITEM_BIT(item_ty)   (1UL << (uint32_t)(item_ty))

_Static_assert(FDB_ITEM_TY_MAX <= 32U, "FDB dirty mask supports at most 32 types");

typedef struct {
    uint32_t    offset;
    uint32_t    cap;
    const char *name;
} msg_t;

typedef struct {
    void    *buf;
    uint32_t buf_size;
    bool     update;
    bool     registered;
    const msg_t *msg;
} item_t;

sobi_t g_sobi;

static volatile item_t items[FDB_ITEM_TY_MAX];
static const msg_t msgs[FDB_ITEM_TY_MAX] = {
    [FDB_ITEM_TY_FAC]    = { .offset = FDB_FACTORY_ADDR - FDB_BASE_ADDR,  .cap = FDB_FACTORY_SIZ, .name = "fdb_factory", },
    [FDB_ITEM_TY_CALIB]  = { .offset = FDB_CALIB_ADDR - FDB_BASE_ADDR,    .cap = FDB_CALIB_SIZ,   .name = "fdb_calib",   },
    [FDB_ITEM_TY_COMMON] = { .offset = FDB_COMMON_ADDR - FDB_BASE_ADDR,   .cap = FDB_COMMON_SIZ,  .name = "fdb_common",  },
    [FDB_ITEM_TY_SOBI]   = { .offset = FDB_SOBI_ADDR - FDB_BASE_ADDR,     .cap = FDB_SOBI_SIZ,     .name = "fdb_sobi",   },
    [FDB_ITEM_TY_MAP]    = { .offset = FDB_MAP_ADDR - FDB_BASE_ADDR,      .cap = FDB_MAP_SIZ,     .name = "fdb_map",     },
    [FDB_ITEM_TY_ADV]    = { .offset = FDB_ADV_ADDR - FDB_BASE_ADDR,      .cap = FDB_ADV_SIZ,     .name = "fdb_adv",     },
    [FDB_ITEM_TY_RGB]    = { .offset = FDB_RGB_ADDR - FDB_BASE_ADDR,      .cap = FDB_RGB_SIZ,     .name = "fdb_rgb",     },
    [FDB_ITEM_TY_MACRO]  = { .offset = FDB_MACRO_ADDR - FDB_BASE_ADDR,    .cap = FDB_MACRO_SIZ,   .name = "fdb_macro",   },
};

static TX_THREAD fdb_thread;
static TX_MUTEX s_api_mutex;
static TX_MUTEX s_data_mutex;
static TX_SEMAPHORE s_wake_sem;
static TX_SEMAPHORE s_done_sem;
static uint8_t fdb_thread_stack[FDB_TASK_STACK] __attribute__((aligned(8U)));


static uint32_t s_generation[FDB_ITEM_TY_MAX];
static rot_state_t s_save_result[FDB_ITEM_TY_MAX];

static volatile uint32_t s_dirty_mask;
static volatile uint32_t s_now_mask;
static bool s_initialized;

static bool _mutex_lock(TX_MUTEX *mutex) {
    return (TX_SUCCESS == tx_mutex_get(mutex, TX_NO_WAIT)) || (TX_SUCCESS == tx_mutex_get(mutex, TX_WAIT_FOREVER));
}
static void _mutex_unlock(TX_MUTEX *mutex) {
    (void)tx_mutex_put(mutex);
}
static bool _sem_wait(TX_SEMAPHORE *semaphore, ULONG timeout) {
    return TX_SUCCESS == tx_semaphore_get(semaphore, timeout);
}
static void _sem_post(TX_SEMAPHORE *semaphore) {
    (void)tx_semaphore_ceiling_put(semaphore, 1U);
}
static void _sem_reset(TX_SEMAPHORE *semaphore) {
    while (TX_SUCCESS == tx_semaphore_get(semaphore, TX_NO_WAIT)) {}
}

static bool _type_valid(fdb_item_ty type) 
{
    return (uint32_t)type < (uint32_t)FDB_ITEM_TY_MAX;
}

static void *_registered_record(fdb_item_ty type)
{
    if (!_type_valid(type) || !items[type].registered) {
        return NULL;
    }
    return items[type].buf;
}

static void _state_snapshot(uint32_t *dirty, uint32_t *now)
{
    *dirty = 0U;
    *now = 0U;
    if (!_mutex_lock(&s_data_mutex)) {
        return;
    }
    *dirty = s_dirty_mask;
    *now = s_now_mask;
    _mutex_unlock(&s_data_mutex);
}

static bool _type_is_dirty(fdb_item_ty type)
{
    bool dirty;

    if (!_mutex_lock(&s_data_mutex)) {
        return false;
    }
    dirty = (s_dirty_mask & FDB_ITEM_BIT(type)) != 0U;
    _mutex_unlock(&s_data_mutex);
    return dirty;
}

static rot_state_t _snapshot_type(fdb_item_ty type, fdb_save_ty save_type)
{
    if (_registered_record(type) == NULL) {
        return rot_state_fdb_not_registered;
    }
    if (!_mutex_lock(&s_data_mutex)) {
        return rot_state_fdb_lock_fail;
    }

    s_generation[type]++;
    s_dirty_mask |= FDB_ITEM_BIT(type);
    if (save_type == FDB_SAVE_TY_NOW) {
        s_now_mask |= FDB_ITEM_BIT(type);
    } else {
        s_now_mask &= ~FDB_ITEM_BIT(type);
    }

    _mutex_unlock(&s_data_mutex);
    return rot_state_success;
}

static rot_state_t _item_read(fdb_item_ty type)
{
    if (!_type_valid(type)) {
        return rot_state_fdb_invalid_type;
    }

    rot_state_t result;
    const volatile item_t *item = &items[type];

    memset(item->buf, 0, item->buf_size);
    result = hal_flash_read(item->msg->offset, item->buf, item->buf_size);
    if (result != rot_state_success) {
        printf(TAG "read %s failed: 0x%08x\r\n", item->msg->name, (unsigned)result);
        return result;
    }

    printf(TAG "%s loaded %u byte(s)\r\n", item->msg->name, (unsigned)item->buf_size);
    return rot_state_success;
}

static rot_state_t _item_write(fdb_item_ty type)
{
    const volatile item_t *item;
    const msg_t *msg;
    rot_state_t result;
    bool equal = false;

    if (!_type_valid(type)) {
        return rot_state_fdb_invalid_type;
    }
    item = &items[type];
    msg = item->msg;
    if ((!item->registered) || (item->buf == NULL) || (msg == NULL) || (item->buf_size == 0U) ||
        ((item->buf_size & 3U) != 0U) || (item->buf_size > msg->cap)) {
        return rot_state_invalid_argument;
    }

    uint32_t erase_size = ROT_ALIGN_UP(item->buf_size, FDB_PAGE_SIZ);
    result = hal_flash_compare(msg->offset, item->buf, item->buf_size, &equal);
    if (result != rot_state_success) {
        return result;
    }
    if (equal) {
        return rot_state_success;
    }

    result = hal_flash_erase(msg->offset, erase_size);
    if (result == rot_state_success) {
        result = hal_flash_write(msg->offset, item->buf, item->buf_size);
    }
    return result;
}

static rot_state_t _save_type(fdb_item_ty type)
{
    const volatile item_t *item = &items[type];
    uint32_t type_bit = FDB_ITEM_BIT(type);
    uint32_t generation;
    rot_state_t result;

    if ((!item->registered) || (item->buf == NULL) || (item->msg == NULL) || (item->buf_size == 0U) || 
        ((item->buf_size & 3U) != 0U) || (item->buf_size > item->msg->cap)) {
        return rot_state_fdb_save_fail;
    }
    if (!_mutex_lock(&s_data_mutex)) {
        return rot_state_fdb_lock_fail;
    }
    generation = s_generation[type];
    _mutex_unlock(&s_data_mutex);

    result = _item_write(type);

    if (!_mutex_lock(&s_data_mutex)) {
        return rot_state_fdb_lock_fail;
    }
    if (s_generation[type] == generation) {
        s_dirty_mask &= ~type_bit;
        s_now_mask &= ~type_bit;
    }
    s_save_result[type] = result;
    _mutex_unlock(&s_data_mutex);

    if (result != rot_state_success) {
        printf(TAG "save %s failed: 0x%08x\r\n", item->msg->name, (unsigned)result);
    }
    return result;
}

static void fdb_thread_entry(ULONG argument)
{
    (void)argument;

    for (;;) {
        uint32_t dirty;
        uint32_t now_mask;
        bool saved_now = false;

        (void)_sem_wait(&s_wake_sem, TX_WAIT_FOREVER);

        for (;;) {
            _state_snapshot(&dirty, &now_mask);
            if (dirty == 0U) {
                break;
            }

            saved_now = false;
            for (uint32_t i = 0U; i < (uint32_t)FDB_ITEM_TY_MAX; i++) {
                if (((dirty & FDB_ITEM_BIT(i)) != 0U) &&
                    ((now_mask & FDB_ITEM_BIT(i)) != 0U)) {
                    (void)_save_type((fdb_item_ty)i);
                    saved_now = true;
                }
            }
            if (saved_now) {
                _sem_post(&s_done_sem);
                continue;
            }

            if (_sem_wait(&s_wake_sem, FDB_SAVE_DELAY_MS)) {
                continue;
            }

            _state_snapshot(&dirty, &now_mask);
            if ((dirty & now_mask) != 0U) {
                continue;
            }
            for (uint32_t i = 0U; i < (uint32_t)FDB_ITEM_TY_MAX; i++) {
                if ((dirty & FDB_ITEM_BIT(i)) != 0U) {
                    (void)_save_type((fdb_item_ty)i);
                }
            }
            break;
        }
    }
}

rot_state_t fdb_register(fdb_item_ty type, void *buf, uint32_t buf_size)
{
    volatile item_t *item;
    rot_state_t result;

    if (!s_initialized) {
        return rot_state_fdb_not_init;
    }
    if (!_type_valid(type)) {
        return rot_state_fdb_invalid_type;
    }
    if ((buf == NULL) || (buf_size == 0U) || ((buf_size & 3U) != 0U) || (buf_size > msgs[type].cap)) {
        return rot_state_invalid_argument;
    }

    item = &items[type];
    if (item->registered) {
        return ((item->buf == buf) && (item->buf_size == buf_size)) ? rot_state_success : rot_state_invalid_argument;
    }

    item->buf        = buf;
    item->buf_size   = buf_size;
    item->update     = false;
    item->registered = true;
    item->msg        = &msgs[type];

    s_generation[type]  = 0U;
    s_save_result[type] = rot_state_success;

    result = _item_read(type);
    if (result != rot_state_success) {
        item->buf = NULL;
        item->buf_size = 0U;
        item->update = false;
        item->registered = false;
        item->msg = NULL;
    }
    return result;
}

rot_state_t fdb_update(fdb_item_ty type, fdb_save_ty save_type)
{
    bool now;
    rot_state_t result;

    if (!s_initialized) {
        return rot_state_fdb_not_init;
    }
    if (!_type_valid(type)) {
        return rot_state_fdb_invalid_type;
    }
    if ((uint32_t)save_type >= (uint32_t)FDB_SAVE_TY_MAX) {
        return rot_state_fdb_invalid_save_type;
    }
    if (!_mutex_lock(&s_api_mutex)) {
        return rot_state_fdb_lock_fail;
    }

    now = save_type == FDB_SAVE_TY_NOW;
    if (now) {
        _sem_reset(&s_done_sem);
    }

    result = _snapshot_type(type, save_type);
    if (result == rot_state_success) {
        _sem_post(&s_wake_sem);
    }

    if ((result == rot_state_success) && now) {
        while (_type_is_dirty(type)) {
            if (!_sem_wait(&s_done_sem, FDB_NOW_TIMEOUT_MS)) {
                result = rot_state_timeout;
                break;
            }
        }
        if (result == rot_state_success) {
            if (!_mutex_lock(&s_data_mutex)) {
                result = rot_state_fdb_lock_fail;
            } else {
                result = s_save_result[type];
                _mutex_unlock(&s_data_mutex);
            }
        }
    }

    _mutex_unlock(&s_api_mutex);
    return result;
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
    if ((TX_SUCCESS != tx_mutex_create(&s_api_mutex, "fdb_api", TX_INHERIT)) ||
        (TX_SUCCESS != tx_mutex_create(&s_data_mutex, "fdb_data", TX_INHERIT)) ||
        (TX_SUCCESS != tx_semaphore_create(&s_wake_sem, "fdb_wake", 0U)) ||
        (TX_SUCCESS != tx_semaphore_create(&s_done_sem, "fdb_done", 0U))) {
        return rot_state_fdb_init_fail;
    }

    s_dirty_mask = 0U;
    s_now_mask = 0U;

    if (TX_SUCCESS != tx_thread_create(&fdb_thread, "thread_fdb", fdb_thread_entry, 0U, fdb_thread_stack, sizeof(fdb_thread_stack), FDB_TASK_PRIORITY, FDB_TASK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START)) {
        return rot_state_fdb_init_fail;
    }
    s_initialized = true;

    if (s_dirty_mask != 0U) {
        _sem_post(&s_wake_sem);
    }
    return rot_state_success;
}
