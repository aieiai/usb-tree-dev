#include "db.h"
#include "flash_layout.h"
#include "hal_flash.h"
#include "rotver.h"
#include "rotutil.h"

#include "tx_api.h"
#include "tx_config.h"

#define TAG "db: "

#define DB_SAVE_DELAY_MS       (5000U)
#define DB_SAVE_DELAY_TICKS    ((DB_SAVE_DELAY_MS * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U)
#define DB_ITEM_BIT(item_ty)   (1UL << (uint32_t)(item_ty))

_Static_assert(DB_ITEM_MAX <= 32U, "DB dirty mask supports at most 32 types");

typedef struct {
    uint32_t offset;
    uint32_t cap;
    const char *name;
} msg_t;

typedef struct {
    void *buf;
    uint32_t buf_size;
    bool registered;
    const msg_t msg;
} item_t;


static volatile item_t items[DB_ITEM_MAX] = {
    [DB_ITEM_OBFAC] = { .msg = { .offset = FLASH_OBF_ADDR   - FLASH_BASE_ADDR, .cap = FLASH_OBF_SIZ,   .name = "db_obf   - factory"  } },
    [DB_ITEM_OBMAG] = { .msg = { .offset = FLASH_OBMAG_ADDR - FLASH_BASE_ADDR, .cap = FLASH_OBMAG_SIZ, .name = "db_obmag - magnet"   } },
    [DB_ITEM_OBCOM] = { .msg = { .offset = FLASH_OBCOM_ADDR - FLASH_BASE_ADDR, .cap = FLASH_OBCOM_SIZ, .name = "db_obcom - common"   } },
    [DB_ITEM_OBI]   = { .msg = { .offset = FLASH_OBI_ADDR   - FLASH_BASE_ADDR, .cap = FLASH_OBI_SIZ,   .name = "db_obi   - info"     } },
    [DB_ITEM_OBM]   = { .msg = { .offset = FLASH_OBM_ADDR   - FLASH_BASE_ADDR, .cap = FLASH_OBM_SIZ,   .name = "db_obm   - mapping"  } },
    [DB_ITEM_OBA]   = { .msg = { .offset = FLASH_OBA_ADDR   - FLASH_BASE_ADDR, .cap = FLASH_OBA_SIZ,   .name = "db_oba   - advanced" } },
    [DB_ITEM_OBR]   = { .msg = { .offset = FLASH_OBR_ADDR   - FLASH_BASE_ADDR, .cap = FLASH_OBR_SIZ,   .name = "db_obr   - rgb"      } },
    [DB_ITEM_OBKP]  = { .msg = { .offset = FLASH_OBKP_ADDR  - FLASH_BASE_ADDR, .cap = FLASH_OBKP_SIZ,  .name = "db_obkp  - property" } },
};

static TX_THREAD db_thread;
static TX_SEMAPHORE s_wake_sem;
static uint8_t db_thread_stack[DB_THREAD_STACK] __attribute__((aligned(8U)));

static uint32_t s_generation[DB_ITEM_MAX];

static volatile uint32_t s_dirty_mask;
static volatile uint32_t s_now_mask;
static bool s_initialized;

static bool _type_valid(db_item_ty type) 
{
    return (uint32_t)type < (uint32_t)DB_ITEM_MAX;
}

static void *_registered_record(db_item_ty type)
{
    if (!_type_valid(type) || !items[type].registered) {
        return NULL;
    }
    return items[type].buf;
}

static void _state_snapshot(uint32_t *dirty, uint32_t *now)
{
    TX_INTERRUPT_SAVE_AREA

    TX_DISABLE
    *dirty = s_dirty_mask;
    *now = s_now_mask;
    TX_RESTORE
}

static rotsta_t _snapshot_type(db_item_ty type, db_save_ty save_type)
{
    uint32_t type_bit = DB_ITEM_BIT(type);
    TX_INTERRUPT_SAVE_AREA

    if (_registered_record(type) == NULL) {
        return rotsta_db_not_registered;
    }

    TX_DISABLE
    s_generation[type]++;
    s_dirty_mask |= type_bit;
    if (save_type == DB_SAVE_TY_NOW) {
        s_now_mask |= type_bit;
    }
    TX_RESTORE

    return rotsta_success;
}

static rotsta_t _item_read(db_item_ty type)
{
    if (!_type_valid(type)) {
        return rotsta_db_invalid_type;
    }

    rotsta_t result;
    const volatile item_t *item = &items[type];

    memset(item->buf, 0, item->buf_size);
    result = hal_flash_read(item->msg.offset, item->buf, item->buf_size);
    if (result != rotsta_success) {
        printf(TAG "read %s failed: 0x%08x\r\n", item->msg.name, (unsigned)result);
        return result;
    }

    printf(TAG "%s loaded %u byte\r\n", item->msg.name, (unsigned)item->buf_size);
    return rotsta_success;
}

static rotsta_t _item_write(db_item_ty type)
{
    const volatile item_t *item;
    rotsta_t result;
    bool equal = false;

    if (!_type_valid(type)) {
        return rotsta_db_invalid_type;
    }
    item = &items[type];
    if ((!item->registered) || (item->buf == NULL) || (item->buf_size == 0U) || ((item->buf_size & 3U) != 0U) || (item->buf_size > item->msg.cap)) {
        return rotsta_invalid_argument;
    }

    uint32_t erase_size = ROT_ALIGN_UP(item->buf_size, FLASH_SECTOR_SIZ);
    result = hal_flash_compare(item->msg.offset, item->buf, item->buf_size, &equal);
    if (result != rotsta_success) {
        printf(TAG "%s compare failed: 0x%08x\r\n", item->msg.name, (unsigned)result);
        return result;
    }
    if (equal) {
        printf(TAG "%s not changed\r\n", item->msg.name);
        return rotsta_success;
    }

    result = hal_flash_erase(item->msg.offset, erase_size);
    if (result != rotsta_success) {
        printf(TAG "%s erase failed: 0x%08x\r\n", item->msg.name, (unsigned)result);
        return result;
    }

    result = hal_flash_write(item->msg.offset, item->buf, item->buf_size);
    if (result != rotsta_success) {
        printf(TAG "%s write failed: 0x%08x\r\n", item->msg.name, (unsigned)result);
        return result;
    }
    printf(TAG "%s written %u byte\r\n", item->msg.name, (unsigned)item->buf_size);
    return result;
}

static rotsta_t _save_type(db_item_ty type)
{
    const volatile item_t *item = &items[type];
    uint32_t type_bit = DB_ITEM_BIT(type);
    uint32_t generation;
    rotsta_t result;
    TX_INTERRUPT_SAVE_AREA

    if ((!item->registered) || (item->buf == NULL) || (item->buf_size == 0U) || ((item->buf_size & 3U) != 0U) || (item->buf_size > item->msg.cap)) {
        return rotsta_db_save_fail;
    }

    TX_DISABLE
    generation = s_generation[type];
    TX_RESTORE

    result = _item_write(type);

    TX_DISABLE
    /* Only clear the pending flags when no newer update happened during the write.
     * A failed write keeps the dirty flag so the item is retried on the next cycle. */
    if ((result == rotsta_success) && (s_generation[type] == generation)) {
        s_dirty_mask &= ~type_bit;
        s_now_mask &= ~type_bit;
    }
    TX_RESTORE

    if (result != rotsta_success) {
        printf(TAG "save %s failed: 0x%08x, retry later\r\n", item->msg.name, (unsigned)result);
    }
    return result;
}

static void _flush_dirty(void)
{
    uint32_t dirty;
    TX_INTERRUPT_SAVE_AREA

    /* A NOW request is satisfied once this batch is attempted; failed items keep
     * their dirty bit and are retried after the normal delay. */
    TX_DISABLE
    dirty = s_dirty_mask;
    s_now_mask = 0U;
    TX_RESTORE

    for (uint32_t i = 0U; i < (uint32_t)DB_ITEM_MAX; i++) {
        if ((dirty & DB_ITEM_BIT(i)) != 0U) {
            (void)_save_type((db_item_ty)i);
        }
    }
}

static void db_thread_entry(ULONG argument)
{
    (void)argument;

    for (;;) {
        (void)tx_semaphore_get(&s_wake_sem, TX_WAIT_FOREVER);

        for (;;) {
            uint32_t dirty, now_mask;

            _state_snapshot(&dirty, &now_mask);
            if (dirty == 0U) {
                break;
            }

            if (now_mask != 0U) {
                printf(TAG "now: \r\n");
                _flush_dirty();
                continue;
            }

            /* Delayed save: every new request restarts the 5s window. */
            ULONG delay_start = tx_time_get();
            UINT delay_ret = tx_semaphore_get(&s_wake_sem, DB_SAVE_DELAY_TICKS);
            if (TX_SUCCESS == delay_ret) {
                continue;
            }

            _state_snapshot(&dirty, &now_mask);
            if (now_mask != 0U) {
                continue;
            }
            printf(TAG "d5s: ret=%u elapsed=%u tick\r\n", (unsigned)delay_ret, (unsigned)(tx_time_get() - delay_start));
            _flush_dirty();
        }
    }
}

rotsta_t db_update(db_item_ty type, db_save_ty save_type)
{
    rotsta_t result;

    if (!s_initialized) {
        return rotsta_db_not_init;
    }
    if (!_type_valid(type)) {
        return rotsta_db_invalid_type;
    }
    if (save_type >= DB_SAVE_TY_MAX) {
        return rotsta_db_invalid_save_type;
    }

    /* Non-blocking: only mark the item dirty and wake the saver. NOW is flushed as soon as the saver runs; D5S after 5s without further updates. */
    result = _snapshot_type(type, save_type);
    if (result != rotsta_success) {
        return result;
    }

    if (TX_NULL == tx_thread_identify()) {
        /* Kernel not started yet (e.g. tx_application_define): db_thread cannot run,
         * so flush here to make boot-time updates durable before continuing. */
        // printf(TAG "db_update %s %u\r\n", items[type].msg.name, (unsigned)save_type);
        uint32_t dirty;
        TX_INTERRUPT_SAVE_AREA

        _flush_dirty();

        TX_DISABLE
        dirty = s_dirty_mask;
        TX_RESTORE

        if (dirty != 0U) {
            tx_semaphore_ceiling_put(&s_wake_sem, 1U); /* Save failed: let db_thread retry once the kernel is running. */
        }
        return ((dirty & DB_ITEM_BIT(type)) == 0U) ? rotsta_success : rotsta_db_save_fail;
    }

    tx_semaphore_ceiling_put(&s_wake_sem, 1U);
    return rotsta_success;
}

rotsta_t db_register(db_item_ty type, void *buf, uint32_t buf_size)
{
    volatile item_t *item;
    rotsta_t result;

    if (!s_initialized) {
        return rotsta_db_not_init;
    }
    if (!_type_valid(type)) {
        return rotsta_db_invalid_type;
    }
    if ((buf == NULL) || (buf_size == 0U) || ((buf_size & 3U) != 0U) || (buf_size > items[type].msg.cap)) {
        return rotsta_invalid_argument;
    }

    item = &items[type];
    if (item->registered) {
        return ((item->buf == buf) && (item->buf_size == buf_size)) ? rotsta_success : rotsta_invalid_argument;
    }

    item->buf        = buf;
    item->buf_size   = buf_size;
    item->registered = true;

    s_generation[type] = 0U;

    result = _item_read(type);
    if (result != rotsta_success) {
        item->buf = NULL;
        item->buf_size = 0U;
        item->registered = false;
    }
    return result;
}

rotsta_t db_init(void)
{
    rotsta_t result;

    if (s_initialized) {
        return rotsta_success;
    }

    result = hal_flash_init();
    if (result != rotsta_success) {
        return rotsta_db_init_fail;
    }

    if (TX_SUCCESS != tx_semaphore_create(&s_wake_sem, "db_wake", 0U)) {
        return rotsta_db_init_fail;
    }

    s_dirty_mask = 0U;
    s_now_mask = 0U;

    if (TX_SUCCESS != tx_thread_create(&db_thread, "db_thread", db_thread_entry, 0U, db_thread_stack, sizeof(db_thread_stack), DB_THREAD_PRIORITY, DB_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START)) {
        return rotsta_db_init_fail;
    }

    s_initialized = true;
    result = db_register_kbd();
    if (result != rotsta_success) {
        return rotsta_db_register_fail;
    }

    return rotsta_success;
}
