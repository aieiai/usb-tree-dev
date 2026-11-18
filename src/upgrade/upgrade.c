#include "upgrade.h"

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "hal_flash.h"
#include "flash_layout.h"
#include "hpm_ppor_drv.h"
#include "hpm_romapi.h"
#include "rotutil.h"
#include "rotcrc.h"
#include "rotver.h"


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


typedef struct {
    bool active;
    uint8_t slot_act;
    uint8_t slot_tag;
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
static upgrade_ctx_t ctx;
static uint8_t s_align_buf[4] __attribute__((aligned(4)));
static uint8_t s_align_count;
static uint32_t s_programmed_size;
static uint8_t s_io_buf[UPGRADE_IO_CHUNK_SIZE] __attribute__((aligned(4)));



static bool _slot_valid(uint8_t slot)
{
    return slot < FLASH_APP_IMG_NUM;
}

static uint32_t _slot_offset(uint8_t slot)
{
    return ((slot == 0U) ? FLASH_APP1_IMG_ADDR : FLASH_APP2_IMG_ADDR) - FLASH_BASE_ADDR;
}

static uint32_t _boot_header_offset(uint8_t slot)
{
    return _slot_offset(slot) + UPGRADE_BOOT_HEAD_OFFSET;
}

static bool _boot_header_valid(const uint8_t *header, uint16_t *sw_version)
{
    uint16_t length = rotutil_get_le16(&header[2]);
    uint8_t fw_count = header[11];

    if ((header[0] != UPGRADE_BOOT_HEAD_TAG) || (header[1] != UPGRADE_BOOT_HEAD_VERSION) || (length < UPGRADE_BOOT_HEAD_SIZE) ||
        (length > 0x2000U) || (fw_count < 1U) || (fw_count > 2U)) {
        return false;
    }

    *sw_version = rotutil_get_le16(&header[UPGRADE_SW_VERSION_OFFSET]);
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

    if ((sec_img_offset == 0U) || (offset != (FLASH_APP2_IMG_ADDR - FLASH_BASE_ADDR)) || (size != FLASH_APP_IMG_SIZE)) {
        printf(TAG "OTP boot layout mismatch: offset=%u, size=%u\r\n", (unsigned)offset, (unsigned)size);
        return rotsta_upgrade_boot_layout_mismatch;
    }
    return rotsta_success;
}

static void _reset_ctx(void)
{
    uint8_t slot_act = ctx.slot_act;

    memset(&ctx, 0, sizeof(ctx));
    ctx.slot_act = slot_act;
    s_align_count = 0U;
    s_programmed_size = 0U;
}

/* Any failure invalidates the target slot so it can never be booted. */
static rotsta_t _fail(rotsta_t state)
{
    rotsta_t cleanup = rotsta_success;

    if (ctx.active) {
        cleanup = hal_flash_erase(ctx.slot_offset, FLASH_APP_IMG_SIZE);
    }
    _reset_ctx();
    return (cleanup == rotsta_success) ? state : cleanup;
}

static void _capture_boot_header(uint32_t image_offset, const uint8_t *data, uint32_t len)
{
    uint32_t head = ctx.file_boot_offset;
    uint32_t head_end = head + UPGRADE_BOOT_HEAD_SIZE;
    uint32_t end = image_offset + len;
    uint32_t start;
    uint32_t stop;

    if ((end <= head) || (image_offset >= head_end)) {
        return;
    }
    start = (image_offset > head) ? image_offset : head;
    stop = (end < head_end) ? end : head_end;
    memcpy(&ctx.boot_header[start - head], &data[start - image_offset], stop - start);
    if ((stop - head) > ctx.boot_collected) {
        ctx.boot_collected = (uint16_t)(stop - head);
    }
}

/*
 * Programs one 4-byte aligned image range while keeping the boot header erased.
 * It is only committed after the whole image has been verified.
 */
static rotsta_t _program_image(uint32_t image_offset, const uint8_t *data, uint32_t len)
{
    uint32_t dst = ctx.slot_offset + ctx.image_base_offset + image_offset;
    uint32_t end = dst + len;
    uint32_t head = ctx.slot_offset + UPGRADE_BOOT_HEAD_OFFSET;
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
    uint32_t head = ctx.file_boot_offset;
    uint32_t head_end = head + UPGRADE_BOOT_HEAD_SIZE;

    while (consumed < ctx.image_size) {
        uint32_t remaining = ctx.image_size - consumed;
        uint32_t read_size = ROT_ALIGN_UP(remaining, 4U);
        uint32_t sum_size;
        uint32_t start;
        uint32_t stop;
        rotsta_t state;

        if (read_size > sizeof(s_io_buf)) {
            read_size = sizeof(s_io_buf);
        }
        sum_size = (remaining < read_size) ? remaining : read_size;

        state = hal_flash_read(ctx.slot_offset + ctx.image_base_offset + consumed, s_io_buf, read_size);
        if (state != rotsta_success) {
            return state;
        }

        if ((ctx.boot_collected == UPGRADE_BOOT_HEAD_SIZE) && (head_end > consumed) && (head < (consumed + sum_size))) {
            start = (head > consumed) ? head : consumed;
            stop = (head_end < (consumed + sum_size)) ? head_end : (consumed + sum_size);
            memcpy(&s_io_buf[start - consumed], &ctx.boot_header[start - head], stop - start);
        }

        crc = rotcrc32_update(crc, s_io_buf, sum_size);
        consumed += sum_size;
    }

    crc = ~crc;
    if ((crc != ctx.image_crc32) || (crc != ~ctx.received_crc32)) {
        printf(TAG "crc mismatch: flash 0x%08x, start 0x%08x, stream 0x%08x\r\n", (unsigned)crc, (unsigned)ctx.image_crc32, (unsigned)~ctx.received_crc32);
        return rotsta_upgrade_checksum_mismatch;
    }
    return rotsta_success;
}

static rotsta_t _commit_boot_header(void)
{
    uint32_t offset = _boot_header_offset(ctx.slot_tag);
    rotsta_t state;
    bool equal = false;

    state = hal_flash_write(offset, ctx.boot_header, sizeof(ctx.boot_header));
    if (state == rotsta_success) {
        state = hal_flash_compare(offset, ctx.boot_header, sizeof(ctx.boot_header), &equal);
    }
    if ((state != rotsta_success) || !equal) {
        return (state != rotsta_success) ? state : rotsta_upgrade_verify_fail;
    }
    return rotsta_success;
}

/* NOR allows 1 -> 0, so retiring a slot needs no erase of the running image. */
static rotsta_t _retire_active_slot(void)
{
    uint32_t offset = _boot_header_offset(ctx.slot_act) + UPGRADE_SW_VERSION_OFFSET;
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
    if (hal_flash_get_sector_size() != FLASH_SECTOR_SIZ) {
        printf(TAG "sector size %u does not match DB page size %u\r\n", (unsigned)hal_flash_get_sector_size(), (unsigned)FLASH_SECTOR_SIZ);
        return rotsta_upgrade_init_fail;
    }

    state = hal_flash_get_active_app_slot(&active_slot);
    if (state != rotsta_success) {
        return state;
    }
    if (!_slot_valid(active_slot)) {
        return rotsta_upgrade_init_fail;
    }

    _reset_ctx();
    ctx.slot_act = active_slot;
    s_initialized = true;

    printf(TAG "active APP%u, exip=%u\r\n", (unsigned)(ctx.slot_act + 1U), _exip_enabled() ? 1U : 0U);
    return rotsta_success;
}

rotsta_t upgrade_get_info(upgrade_info_t *info)
{
    uint32_t otp_boot;
    uint32_t sec_img_offset;
    uint32_t max_img_len;

    if (info == NULL) {
        return rotsta_invalid_argument;
    }
    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }

    memset(info, 0, sizeof(*info));
    info->slot_act = ctx.slot_act;
    info->slot_tag = (uint8_t)(ctx.slot_act ^ 1U);
    info->slot_cnt = FLASH_APP_IMG_NUM;
    info->encrypted = _exip_enabled() ? 1U : 0U;
    info->remap_enabled = (ctx.slot_act == 1U) ? 1U : 0U;
    info->slot_size = FLASH_APP_IMG_SIZE;
    info->image_base_offset = info->encrypted ? 0U : UPGRADE_IMAGE_BASE_PLAIN;
    info->file_boot_offset = info->encrypted
        ? UPGRADE_BOOT_HEAD_OFFSET
        : (UPGRADE_BOOT_HEAD_OFFSET - UPGRADE_IMAGE_BASE_PLAIN);
    info->firmware_version = FW_VER;

    otp_boot = rom_otp_read_from_shadow(OTP_BOOT_WORD_INDEX);
    sec_img_offset = otp_boot & OTP_SEC_IMG_OFFSET_MASK;
    max_img_len = otp_boot >> OTP_MAX_IMG_LEN_SHIFT;
    info->otp_sec_img_offset = (uint16_t)sec_img_offset;
    info->otp_max_img_len = (uint16_t)max_img_len;
    info->otp_offset = sec_img_offset * OTP_SEC_IMG_UNIT;
    info->otp_size = ((max_img_len != 0U) ? max_img_len : sec_img_offset) * OTP_SEC_IMG_UNIT;
    info->boot_ota_enabled = (sec_img_offset != 0U) ? 1U : 0U;
    info->otp_address_match =
        (info->otp_offset == (FLASH_APP2_IMG_ADDR - FLASH_BASE_ADDR)) ? 1U : 0U;
    info->otp_length_match = (info->otp_size == FLASH_APP_IMG_SIZE) ? 1U : 0U;

    for (uint8_t slot = 0U; slot < FLASH_APP_IMG_NUM; slot++) {
        uint16_t version = 0U;
        rotsta_t state = _read_slot_version(slot, &version);

        if (state == rotsta_upgrade_invalid_image) {
            continue;
        }
        if (state != rotsta_success) {
            return state;
        }
        info->slot_valid[slot] = 1U;
        info->slot_version[slot] = version;
        info->slot_retired[slot] = (version == 0U) ? 1U : 0U;
    }
    return rotsta_success;
}

rotsta_t upgrade_start(uint32_t image_size, uint32_t image_crc32)
{
    uint8_t active_slot = 0U;
    uint8_t slot_tag;
    uint32_t capacity;
    uint16_t active_version = 0U;
    rotsta_t state;

    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }
    if (ctx.active) {
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
    slot_tag = (uint8_t)(active_slot ^ 1U);

    _reset_ctx();
    ctx.slot_act = active_slot;
    ctx.slot_tag = slot_tag;
    ctx.encrypted = _exip_enabled() ? 1U : 0U;
    ctx.slot_offset = _slot_offset(slot_tag);
    ctx.image_base_offset = ctx.encrypted ? 0U : UPGRADE_IMAGE_BASE_PLAIN;
    ctx.file_boot_offset = ctx.encrypted ? UPGRADE_BOOT_HEAD_OFFSET : (UPGRADE_BOOT_HEAD_OFFSET - UPGRADE_IMAGE_BASE_PLAIN);
    capacity = FLASH_APP_IMG_SIZE - ctx.image_base_offset;

    if ((image_size == 0U) || ((ctx.file_boot_offset + UPGRADE_BOOT_HEAD_SIZE) > image_size)) {
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

    ctx.image_size = image_size;
    ctx.image_crc32 = image_crc32;
    ctx.received_crc32 = 0xffffffffUL;
    ctx.active_version = active_version;
    memset(ctx.boot_header, 0xff, sizeof(ctx.boot_header));
    ctx.active = true;

    /* Erase the whole slot so stale EXIP data can never precede a plain image. */
    state = hal_flash_erase(ctx.slot_offset, FLASH_APP_IMG_SIZE);
    if (state != rotsta_success) {
        return _fail(state);
    }

    printf(TAG "APP%u->APP%u size=%u encrypted=%u base=0x%x boot=0x%x\r\n", (unsigned)(ctx.slot_act + 1U), (unsigned)(slot_tag + 1U), (unsigned)image_size, 
                            (unsigned)ctx.encrypted, (unsigned)ctx.image_base_offset, (unsigned)ctx.file_boot_offset);
    return rotsta_success;
}

rotsta_t upgrade_write(const void *data, uint32_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    rotsta_t state;

    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }
    if (!ctx.active) {
        return rotsta_upgrade_incomplete;
    }
    if ((bytes == NULL) || (len == 0U)) {
        return rotsta_invalid_argument;
    }
    if (len > (ctx.image_size - ctx.received_size)) {
        return _fail(rotsta_upgrade_image_too_large);
    }

    _capture_boot_header(ctx.received_size, bytes, len);
    state = _write_image_bytes(bytes, len);
    if (state != rotsta_success) {
        return _fail(state);
    }

    ctx.received_crc32 = rotcrc32_update(ctx.received_crc32, bytes, len);
    ctx.received_size += len;
    return rotsta_success;
}

rotsta_t upgrade_finish(void)
{
    uint16_t sw_version = 0U;
    rotsta_t state;

    if (!s_initialized) {
        return rotsta_upgrade_not_init;
    }
    if (!ctx.active) {
        return rotsta_upgrade_incomplete;
    }
    if (ctx.received_size != ctx.image_size) {
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
    if ((ctx.boot_collected != UPGRADE_BOOT_HEAD_SIZE) || !_boot_header_valid(ctx.boot_header, &sw_version) || (sw_version == 0U)) {
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
    if (sw_version < ctx.active_version) {
        state = _retire_active_slot();
        if (state != rotsta_success) {
            return _fail(state);
        }
        printf(TAG "retired APP%u: SW_VERSION %u -> 0\r\n", (unsigned)(ctx.slot_act + 1U), (unsigned)ctx.active_version);
    }

    printf(TAG "APP%u verified, sw_version=%u, crc=0x%08x\r\n", (unsigned)(ctx.slot_tag + 1U), (unsigned)sw_version, (unsigned)ctx.image_crc32);
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

void upgrade_restart(void)
{
    (void)disable_global_irq(CSR_MSTATUS_MIE_MASK);
    SOC_ROM_FLAG_REG = 0U;
    ppor_sw_reset(HPM_PPOR, 1000U);
    for (;;) {
    }
}
