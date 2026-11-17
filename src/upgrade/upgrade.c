#include "upgrade.h"

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "hal_flash.h"
#include "hpm_ppor_drv.h"
#include "hpm_romapi.h"
#include "rotutil.h"


#define TAG "upgrade: "

#define UPGRADE_IO_CHUNK_SIZE       (256U)
#define UPGRADE_SW_VERSION_OFFSET   (8U)
#define SOC_ROM_FLAG_REG            (*(volatile uint32_t *)(0xF40D4000UL + 8U))

/* HPM5100 OTP word 25: [15:0] SEC_IMG_OFFSET, [31:16] MAX_IMG_LEN, unit 16KB. */
#define OTP_BOOT_WORD_INDEX         (25U)
#define OTP_SECURITY_WORD_INDEX     (24U)
#define OTP_SEC_IMG_OFFSET_MASK     (0xFFFFUL)
#define OTP_MAX_IMG_LEN_SHIFT       (16U)
#define OTP_EXIP_ENABLE_SHIFT       (12U)
#define OTP_EXIP_ENABLE_MASK        (1UL << OTP_EXIP_ENABLE_SHIFT)
#define OTP_SEC_IMG_UNIT            (16U * 1024U)


typedef struct {
    bool active;
    uint8_t target_slot;
    uint8_t encrypted;
    uint32_t slot_offset;
    uint32_t image_base_offset;
    uint32_t file_boot_offset;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t received_size;
    uint32_t received_crc32;
    uint16_t active_version;
    uint16_t boot_collected;
    uint8_t boot_header[UPGRADE_BOOT_HEAD_SIZE];
} upgrade_ctx_t;


static bool s_initialized;
static uint8_t s_active_slot;
static upgrade_ctx_t s_ctx;
static uint8_t s_align_buf[4] __attribute__((aligned(4)));
static uint8_t s_align_count;
static uint32_t s_programmed_size;
static uint8_t s_io_buf[UPGRADE_IO_CHUNK_SIZE] __attribute__((aligned(4)));


static uint16_t _le16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t _crc32_update(uint32_t crc, const void *data, uint32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;

    for (uint32_t i = 0U; i < size; i++) {
        crc ^= bytes[i];
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc >> 1) ^ (0xedb88320UL & (0U - (crc & 1U)));
        }
    }
    return crc;
}

static bool _slot_valid(uint8_t slot)
{
    return slot < FDB_APP_IMG_NUM;
}

static uint32_t _slot_offset(uint8_t slot)
{
    return ((slot == 0U) ? FDB_APP1_IMG_ADDR : FDB_APP2_IMG_ADDR) - FDB_BASE_ADDR;
}

static uint32_t _boot_header_offset(uint8_t slot)
{
    return _slot_offset(slot) + UPGRADE_BOOT_HEAD_OFFSET;
}

static bool _boot_header_valid(const uint8_t *header, uint16_t *sw_version)
{
    uint16_t length = _le16(&header[2]);
    uint8_t fw_count = header[11];

    if ((header[0] != UPGRADE_BOOT_HEAD_TAG) ||
        (header[1] != UPGRADE_BOOT_HEAD_VERSION) ||
        (length < UPGRADE_BOOT_HEAD_SIZE) ||
        (length > 0x2000U) ||
        (fw_count < 1U) ||
        (fw_count > 2U)) {
        return false;
    }

    *sw_version = _le16(&header[UPGRADE_SW_VERSION_OFFSET]);
    return true;
}

static rotsta_t _read_slot_version(uint8_t slot, uint16_t *sw_version)
{
    uint8_t header[UPGRADE_BOOT_HEAD_SIZE] __attribute__((aligned(4)));
    rotsta_t state;

    state = hal_flash_read(_boot_header_offset(slot), header, sizeof(header));
    if (state != rotsta_success) {
        return state;
    }
    if (!_boot_header_valid(header, sw_version)) {
        return rotsta_upgrade_invalid_image;
    }
    return rotsta_success;
}

static bool _exip_enabled(void)
{
    return (rom_otp_read_from_shadow(OTP_SECURITY_WORD_INDEX) & OTP_EXIP_ENABLE_MASK) != 0U;
}

static rotsta_t _check_otp_layout(void)
{
    uint32_t otp_boot = rom_otp_read_from_shadow(OTP_BOOT_WORD_INDEX);
    uint32_t sec_img_offset = otp_boot & OTP_SEC_IMG_OFFSET_MASK;
    uint32_t max_img_len = otp_boot >> OTP_MAX_IMG_LEN_SHIFT;
    uint32_t offset = sec_img_offset * OTP_SEC_IMG_UNIT;
    uint32_t size = ((max_img_len != 0U) ? max_img_len : sec_img_offset) * OTP_SEC_IMG_UNIT;

    if ((sec_img_offset == 0U) ||
        (offset != (FDB_APP2_IMG_ADDR - FDB_BASE_ADDR)) ||
        (size != FDB_APP_IMG_SIZ)) {
        printf(TAG "OTP boot layout mismatch: offset=%u, size=%u\r\n",
               (unsigned)offset, (unsigned)size);
        return rotsta_upgrade_boot_layout_mismatch;
    }
    return rotsta_success;
}

static void _reset_ctx(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_align_count = 0U;
    s_programmed_size = 0U;
}

/* Any failure invalidates the target slot so it can never be booted. */
static rotsta_t _fail(rotsta_t state)
{
    rotsta_t cleanup = rotsta_success;

    if (s_ctx.active) {
        cleanup = hal_flash_erase(s_ctx.slot_offset, FDB_APP_IMG_SIZ);
    }
    _reset_ctx();
    return (cleanup == rotsta_success) ? state : cleanup;
}

static void _capture_boot_header(uint32_t image_offset, const uint8_t *data, uint32_t len)
{
    uint32_t head = s_ctx.file_boot_offset;
    uint32_t head_end = head + UPGRADE_BOOT_HEAD_SIZE;
    uint32_t end = image_offset + len;
    uint32_t start;
    uint32_t stop;

    if ((end <= head) || (image_offset >= head_end)) {
        return;
    }
    start = (image_offset > head) ? image_offset : head;
    stop = (end < head_end) ? end : head_end;
    memcpy(&s_ctx.boot_header[start - head], &data[start - image_offset], stop - start);
    if (stop > s_ctx.boot_collected) {
        s_ctx.boot_collected = (uint16_t)stop;
    }
}

/*
 * Programs one 4-byte aligned image range while keeping the boot header erased.
 * It is only committed after the whole image has been verified.
 */
static rotsta_t _program_image(uint32_t image_offset, const uint8_t *data, uint32_t len)
{
    uint32_t dst = s_ctx.slot_offset + s_ctx.image_base_offset + image_offset;
    uint32_t end = dst + len;
    uint32_t head = s_ctx.slot_offset + UPGRADE_BOOT_HEAD_OFFSET;
    uint32_t head_end = head + UPGRADE_BOOT_HEAD_SIZE;

    if ((end <= head) || (dst >= head_end)) {
        return hal_flash_write(dst, data, len);
    }
    if (dst < head) {
        rotsta_t state = hal_flash_write(dst, data, head - dst);
        if (state != rotsta_success) {
            return state;
        }
    }
    if (end > head_end) {
        return hal_flash_write(head_end, &data[head_end - dst], end - head_end);
    }
    return rotsta_success;
}

static rotsta_t _flush_alignment(uint8_t fill)
{
    rotsta_t state;

    if (s_align_count == 0U) {
        return rotsta_success;
    }

    while (s_align_count < sizeof(s_align_buf)) {
        s_align_buf[s_align_count++] = fill;
    }

    state = _program_image(s_programmed_size, s_align_buf, sizeof(s_align_buf));
    if (state == rotsta_success) {
        s_programmed_size += sizeof(s_align_buf);
        s_align_count = 0U;
    }
    return state;
}

static rotsta_t _write_image_bytes(const uint8_t *data, uint32_t size)
{
    const uint8_t *cursor = data;
    uint32_t remaining = size;
    rotsta_t state;

    while ((remaining > 0U) && (s_align_count < sizeof(s_align_buf))) {
        s_align_buf[s_align_count++] = *cursor++;
        remaining--;
    }
    if (s_align_count == sizeof(s_align_buf)) {
        state = _flush_alignment(0U);
        if (state != rotsta_success) {
            return state;
        }
    }

    if (remaining >= sizeof(s_align_buf)) {
        uint32_t bulk_size = remaining & ~3U;

        state = _program_image(s_programmed_size, cursor, bulk_size);
        if (state != rotsta_success) {
            return state;
        }
        s_programmed_size += bulk_size;
        cursor += bulk_size;
        remaining -= bulk_size;
    }

    while (remaining > 0U) {
        s_align_buf[s_align_count++] = *cursor++;
        remaining--;
    }
    return rotsta_success;
}

/*
 * Reads the stored image back and substitutes the still-erased boot header with
 * the bytes captured from the stream.
 */
static rotsta_t _verify_flash(void)
{
    uint32_t consumed = 0U;
    uint32_t crc = 0xffffffffUL;
    uint32_t head = s_ctx.file_boot_offset;
    uint32_t head_end = head + UPGRADE_BOOT_HEAD_SIZE;

    while (consumed < s_ctx.image_size) {
        uint32_t remaining = s_ctx.image_size - consumed;
        uint32_t read_size = ROT_ALIGN_UP(remaining, 4U);
        uint32_t sum_size;
        uint32_t start;
        uint32_t stop;
        rotsta_t state;

        if (read_size > sizeof(s_io_buf)) {
            read_size = sizeof(s_io_buf);
        }
        sum_size = (remaining < read_size) ? remaining : read_size;

        state = hal_flash_read(s_ctx.slot_offset + s_ctx.image_base_offset + consumed,
                               s_io_buf, read_size);
        if (state != rotsta_success) {
            return state;
        }

        if ((s_ctx.boot_collected == UPGRADE_BOOT_HEAD_SIZE) &&
            (head_end > consumed) && (head < (consumed + sum_size))) {
            start = (head > consumed) ? head : consumed;
            stop = (head_end < (consumed + sum_size)) ? head_end : (consumed + sum_size);
            memcpy(&s_io_buf[start - consumed],
                   &s_ctx.boot_header[start - head],
                   stop - start);
        }

        crc = _crc32_update(crc, s_io_buf, sum_size);
        consumed += sum_size;
    }

    crc = ~crc;
    if ((crc != s_ctx.image_crc32) || (crc != ~s_ctx.received_crc32)) {
        printf(TAG "crc mismatch: flash 0x%08x, start 0x%08x, stream 0x%08x\r\n",
               (unsigned)crc, (unsigned)s_ctx.image_crc32, (unsigned)~s_ctx.received_crc32);
        return rotsta_upgrade_checksum_mismatch;
    }
    return rotsta_success;
}

static rotsta_t _commit_boot_header(void)
{
    uint32_t offset = _boot_header_offset(s_ctx.target_slot);
    rotsta_t state;
    bool equal = false;

    state = hal_flash_write(offset, s_ctx.boot_header, sizeof(s_ctx.boot_header));
    if (state == rotsta_success) {
        state = hal_flash_compare(offset, s_ctx.boot_header,
                                  sizeof(s_ctx.boot_header), &equal);
    }
    if ((state != rotsta_success) || !equal) {
        return (state != rotsta_success) ? state : rotsta_upgrade_verify_fail;
    }
    return rotsta_success;
}

/* NOR allows 1 -> 0, so retiring a slot needs no erase of the running image. */
static rotsta_t _retire_active_slot(void)
{
    uint32_t offset = _boot_header_offset(s_active_slot) + UPGRADE_SW_VERSION_OFFSET;
    uint8_t word[4] __attribute__((aligned(4)));
    rotsta_t state;
    bool equal = false;

    state = hal_flash_read(offset, word, sizeof(word));
    if (state != rotsta_success) {
        return state;
    }
    word[0] = 0U;
    word[1] = 0U;
    state = hal_flash_write(offset, word, sizeof(word));
    if (state == rotsta_success) {
        state = hal_flash_compare(offset, word, sizeof(word), &equal);
    }
    if ((state != rotsta_success) || !equal) {
        return (state != rotsta_success) ? state : rotsta_upgrade_verify_fail;
    }
    return rotsta_success;
}

rotsta_t upgrade_init(void)
{
    rotsta_t state;
    uint8_t active_slot = 0U;

    if (s_initialized) {
        return rotsta_success;
    }

    state = hal_flash_init();
    if (state != rotsta_success) {
        return state;
    }
    if (hal_flash_get_sector_size() != FDB_SECTOR_SIZ) {
        printf(TAG "sector size %u does not match FDB page size %u\r\n",
               (unsigned)hal_flash_get_sector_size(), (unsigned)FDB_SECTOR_SIZ);
        return rotsta_upgrade_init_fail;
    }

    state = hal_flash_get_active_app_slot(&active_slot);
    if (state != rotsta_success) {
        return state;
    }
    if (!_slot_valid(active_slot)) {
        return rotsta_upgrade_init_fail;
    }

    s_active_slot = active_slot;
    _reset_ctx();
    s_initialized = true;

    printf(TAG "active APP%u, exip=%u\r\n",
           (unsigned)(s_active_slot + 1U), _exip_enabled() ? 1U : 0U);
    return rotsta_success;
}

rotsta_t upgrade_get_info(upgrade_info_t *info)
{
    uint16_t version = 0U;

    if (info == NULL) {
        return rotsta_invalid_argument;
    }
    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }

    (void)_read_slot_version(s_active_slot, &version);

    memset(info, 0, sizeof(*info));
    info->active_slot = s_active_slot;
    info->target_slot = (uint8_t)(s_active_slot ^ 1U);
    info->encrypted = _exip_enabled() ? 1U : 0U;
    info->slot_count = FDB_APP_IMG_NUM;
    info->active_version = version;
    info->slot_size = FDB_APP_IMG_SIZ;
    info->image_base_offset = info->encrypted ? 0U : UPGRADE_IMAGE_BASE_PLAIN;
    return rotsta_success;
}

rotsta_t upgrade_start(uint32_t image_size, uint32_t image_crc32)
{
    uint8_t active_slot = 0U;
    uint8_t target_slot;
    uint32_t capacity;
    uint16_t active_version = 0U;
    rotsta_t state;

    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }
    if (s_ctx.active) {
        return rotsta_upgrade_busy;
    }

    state = _check_otp_layout();
    if (state != rotsta_success) {
        return state;
    }

    state = hal_flash_get_active_app_slot(&active_slot);
    if (state != rotsta_success) {
        return state;
    }
    if (!_slot_valid(active_slot)) {
        return rotsta_upgrade_invalid_slot;
    }
    s_active_slot = active_slot;
    target_slot = (uint8_t)(active_slot ^ 1U);

    _reset_ctx();
    s_ctx.target_slot = target_slot;
    s_ctx.encrypted = _exip_enabled() ? 1U : 0U;
    s_ctx.slot_offset = _slot_offset(target_slot);
    s_ctx.image_base_offset = s_ctx.encrypted ? 0U : UPGRADE_IMAGE_BASE_PLAIN;
    s_ctx.file_boot_offset = s_ctx.encrypted
        ? UPGRADE_BOOT_HEAD_OFFSET
        : (UPGRADE_BOOT_HEAD_OFFSET - UPGRADE_IMAGE_BASE_PLAIN);
    capacity = FDB_APP_IMG_SIZ - s_ctx.image_base_offset;

    if ((image_size == 0U) || ((s_ctx.file_boot_offset + UPGRADE_BOOT_HEAD_SIZE) > image_size)) {
        _reset_ctx();
        return rotsta_upgrade_invalid_image;
    }
    if ((image_size > capacity) || (ROT_ALIGN_UP(image_size, 4U) > capacity)) {
        _reset_ctx();
        return rotsta_upgrade_image_too_large;
    }

    /* The running image must have a readable boot header to compare versions. */
    state = _read_slot_version(active_slot, &active_version);
    if (state != rotsta_success) {
        _reset_ctx();
        return state;
    }

    s_ctx.image_size = image_size;
    s_ctx.image_crc32 = image_crc32;
    s_ctx.received_crc32 = 0xffffffffUL;
    s_ctx.active_version = active_version;
    memset(s_ctx.boot_header, 0xff, sizeof(s_ctx.boot_header));
    s_ctx.active = true;

    /* Erase the whole slot so stale EXIP data can never precede a plain image. */
    state = hal_flash_erase(s_ctx.slot_offset, FDB_APP_IMG_SIZ);
    if (state != rotsta_success) {
        return _fail(state);
    }

    printf(TAG "APP%u->APP%u size=%u encrypted=%u base=0x%x boot=0x%x\r\n",
           (unsigned)(s_active_slot + 1U), (unsigned)(target_slot + 1U),
           (unsigned)image_size, (unsigned)s_ctx.encrypted,
           (unsigned)s_ctx.image_base_offset, (unsigned)s_ctx.file_boot_offset);
    return rotsta_success;
}

rotsta_t upgrade_write(const void *data, uint32_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    rotsta_t state;

    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }
    if (!s_ctx.active) {
        return rotsta_upgrade_incomplete;
    }
    if ((bytes == NULL) || (len == 0U)) {
        return rotsta_invalid_argument;
    }
    if (len > (s_ctx.image_size - s_ctx.received_size)) {
        return _fail(rotsta_upgrade_image_too_large);
    }

    _capture_boot_header(s_ctx.received_size, bytes, len);
    state = _write_image_bytes(bytes, len);
    if (state != rotsta_success) {
        return _fail(state);
    }

    s_ctx.received_crc32 = _crc32_update(s_ctx.received_crc32, bytes, len);
    s_ctx.received_size += len;
    return rotsta_success;
}

rotsta_t upgrade_finish(void)
{
    uint16_t sw_version = 0U;
    rotsta_t state;

    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }
    if (!s_ctx.active) {
        return rotsta_upgrade_incomplete;
    }
    if (s_ctx.received_size != s_ctx.image_size) {
        return _fail(rotsta_upgrade_incomplete);
    }

    state = _flush_alignment(0xffU);
    if (state != rotsta_success) {
        return _fail(state);
    }
    state = _verify_flash();
    if (state != rotsta_success) {
        return _fail(state);
    }
    if ((s_ctx.boot_collected != UPGRADE_BOOT_HEAD_SIZE) ||
        !_boot_header_valid(s_ctx.boot_header, &sw_version) ||
        (sw_version == 0U)) {
        return _fail(rotsta_upgrade_invalid_image);
    }

    /* Publish the header only after the image content has been verified. */
    state = _commit_boot_header();
    if (state != rotsta_success) {
        return _fail(state);
    }

    /*
     * BootROM only looks at SW_VERSION. When the new image is older, retire the
     * running slot so the downgrade is selected after the reset.
     */
    if (sw_version < s_ctx.active_version) {
        state = _retire_active_slot();
        if (state != rotsta_success) {
            return _fail(state);
        }
        printf(TAG "retired APP%u: SW_VERSION %u -> 0\r\n",
               (unsigned)(s_active_slot + 1U), (unsigned)s_ctx.active_version);
    }

    printf(TAG "APP%u verified, sw_version=%u, crc=0x%08x\r\n",
           (unsigned)(s_ctx.target_slot + 1U), (unsigned)sw_version,
           (unsigned)s_ctx.image_crc32);
    _reset_ctx();
    return rotsta_success;
}

rotsta_t upgrade_abort(void)
{
    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }

    return _fail(rotsta_success);
}

void upgrade_reboot(void)
{
    printf(TAG "upgrade complete, resetting\r\n");
    (void)disable_global_irq(CSR_MSTATUS_MIE_MASK);
    SOC_ROM_FLAG_REG = 0U;
    ppor_sw_reset(HPM_PPOR, 1000U);
    for (;;) {
    }
}
