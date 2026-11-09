#include "upgrade.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "hal_flash.h"
#include "hpm_ppor_drv.h"
#include "hpm_romapi.h"
#include "rot_ver.h"
#include "rot_utils.h"


#define TAG "upgrade: "

#define UPGRADE_BOOT_HEADER_TAG     (0xbfU)
#define UPGRADE_BOOT_HEADER_MIN_LEN (16U)
#define UPGRADE_BOOT_HEADER_OFFSET  (FDB_APP1_BOOT_HEAD_ADDR - FDB_APP1_IMG_ADDR)
#define UPGRADE_RECORD_OFFSET       (sizeof(upgrade_image_header_t))
#define UPGRADE_RECORD_READ_SIZE    (UPGRADE_RECORD_OFFSET + sizeof(upgrade_record_t))
#define UPGRADE_IO_CHUNK_SIZE       (256U)
#define SOC_ROM_FLAG_REG            (*(volatile uint32_t *)(0xF40D4000UL + 8U))
#define OTP_BOOT_WORD_INDEX         (25U)
#define OTP_SEC_IMG_OFFSET_MASK     (0xFFFFUL)
#define OTP_MAX_IMG_LEN_SHIFT       (16U)

_Static_assert((UPGRADE_RECORD_OFFSET + sizeof(upgrade_record_t)) <= FDB_APP_IMGINFO_SIZ,
               "upgrade metadata does not fit in the image info page");


static bool s_initialized;
static upgrade_state_t s_state;
static uint8_t s_active_slot;
static uint8_t s_target_slot;
static uint32_t s_image_offset;
static uint32_t s_image_size;
static uint32_t s_received_size;
static uint32_t s_programmed_size;
static uint32_t s_expected_checksum;
static uint32_t s_checksum;
static uint32_t s_image_crc32;
static rot_state_t s_last_result;
static upgrade_image_header_t s_header;
static uint8_t s_align_buf[4] __attribute__((aligned(4)));
static uint8_t s_align_count;
static uint8_t s_io_buf[UPGRADE_IO_CHUNK_SIZE] __attribute__((aligned(4)));
static uint8_t s_imginfo_page[FDB_PAGE_SIZ] __attribute__((aligned(4)));


static uint16_t _le16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t _le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
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

static uint32_t _crc32(const void *data, uint32_t size)
{
    uint32_t crc = _crc32_update(0xffffffffUL, data, size);
    return ~crc;
}

static bool _slot_valid(uint8_t slot)
{
    return slot < FDB_APP_IMG_NUM;
}

static uint32_t _slot_image_offset(uint8_t slot)
{
    return ((slot == 0U) ? FDB_APP1_IMG_ADDR : FDB_APP2_IMG_ADDR) - FDB_BASE_ADDR;
}

static uint32_t _slot_imginfo_offset(uint8_t slot)
{
    return ((slot == 0U) ? FDB_APP1_IMGINFO_ADDR : FDB_APP2_IMGINFO_ADDR) - FDB_BASE_ADDR;
}

static rot_state_t _fail(rot_state_t state)
{
    s_state = UPGRADE_STATE_FAILED;
    s_last_result = state;
    return state;
}

static rot_state_t _verify_flash(uint32_t offset, uint32_t size,
                                 uint32_t *checksum, uint32_t *crc32)
{
    uint32_t consumed = 0U;
    uint32_t sum = 0U;
    uint32_t crc = 0xffffffffUL;

    while (consumed < size) {
        uint32_t remaining = size - consumed;
        uint32_t read_size = ROT_ALIGN_UP(remaining, 4U);
        uint32_t sum_size;
        rot_state_t state;

        if (read_size > sizeof(s_io_buf)) {
            read_size = sizeof(s_io_buf);
        }

        state = hal_flash_read(offset + consumed, s_io_buf, read_size);
        if (state != rot_state_success) {
            return state;
        }

        sum_size = (remaining < read_size) ? remaining : read_size;
        for (uint32_t i = 0U; i < sum_size; i++) {
            sum += s_io_buf[i];
        }
        crc = _crc32_update(crc, s_io_buf, sum_size);
        consumed += sum_size;
    }

    *checksum = sum;
    *crc32 = ~crc;
    return rot_state_success;
}

static bool _record_valid(const upgrade_record_t *record, uint8_t slot)
{
    uint32_t crc;

    if ((record == NULL) ||
        (record->magic != UPGRADE_RECORD_MAGIC) ||
        (record->version != UPGRADE_RECORD_VERSION) ||
        (record->size != sizeof(upgrade_record_t)) ||
        (record->slot != slot) ||
        (record->state != UPGRADE_RECORD_STATE_READY) ||
        (record->image_size == 0U) ||
        (record->image_size > FDB_APP_IMG_SIZ) ||
        (record->image_offset != _slot_image_offset(slot))) {
        return false;
    }

    crc = _crc32(record, offsetof(upgrade_record_t, crc32));
    return crc == record->crc32;
}

static rot_state_t _read_boot_header(uint32_t offset, uint32_t image_size,
                                     uint16_t *sw_version, uint16_t *header_length)
{
    rot_state_t state;

    if (image_size < UPGRADE_BOOT_HEADER_MIN_LEN) {
        return rot_state_upgrade_invalid_image;
    }

    state = hal_flash_read(offset + UPGRADE_BOOT_HEADER_OFFSET,
                           s_io_buf,
                           UPGRADE_BOOT_HEADER_MIN_LEN);
    if (state != rot_state_success) {
        return state;
    }
    if (s_io_buf[0] != UPGRADE_BOOT_HEADER_TAG) {
        return rot_state_upgrade_invalid_image;
    }

    uint16_t length = _le16(&s_io_buf[2]);
    if ((length < UPGRADE_BOOT_HEADER_MIN_LEN) || (length > image_size) || (length > 0x2000U)) {
        return rot_state_upgrade_invalid_image;
    }

    *sw_version = _le16(&s_io_buf[8]);
    *header_length = length;
    return rot_state_success;
}


static rot_state_t _read_slot_version(uint8_t slot, uint16_t *sw_version,
                                      uint16_t *header_length, uint8_t *valid)
{
    rot_state_t state;
    uint16_t length;

    *valid = 0U;
    *sw_version = 0U;
    *header_length = 0U;

    state = hal_flash_read(_slot_image_offset(slot) + UPGRADE_BOOT_HEADER_OFFSET,
                           s_io_buf,
                           UPGRADE_BOOT_HEADER_MIN_LEN);
    if (state != rot_state_success) {
        return state;
    }
    if (s_io_buf[0] != UPGRADE_BOOT_HEADER_TAG) {
        return rot_state_success;
    }

    length = _le16(&s_io_buf[2]);
    if ((length < UPGRADE_BOOT_HEADER_MIN_LEN) || (length > 0x2000U)) {
        return rot_state_success;
    }

    *sw_version = _le16(&s_io_buf[8]);
    *header_length = length;
    *valid = 1U;
    return rot_state_success;
}

static rot_state_t _write_record(uint8_t slot, uint32_t image_size,
                                 uint32_t image_checksum, uint32_t image_crc32,
                                 uint16_t sw_version,
                                 uint16_t boot_header_length,
                                 const upgrade_image_header_t *header)
{
    upgrade_record_t record;
    uint32_t imginfo_offset = _slot_imginfo_offset(slot);
    rot_state_t state;
    bool equal = false;

    memset(&record, 0, sizeof(record));
    record.magic = UPGRADE_RECORD_MAGIC;
    record.version = UPGRADE_RECORD_VERSION;
    record.size = sizeof(record);
    record.slot = slot;
    record.state = UPGRADE_RECORD_STATE_READY;
    record.image_size = image_size;
    record.image_checksum = image_checksum;
    record.image_crc32 = image_crc32;
    record.image_offset = _slot_image_offset(slot);
    record.sw_version = sw_version;
    record.boot_header_length = boot_header_length;
    record.crc32 = _crc32(&record, offsetof(upgrade_record_t, crc32));

    state = hal_flash_read(imginfo_offset, s_imginfo_page, sizeof(s_imginfo_page));
    if (state != rot_state_success) {
        return state;
    }
    memcpy(s_imginfo_page, header, sizeof(*header));
    memcpy(&s_imginfo_page[UPGRADE_RECORD_OFFSET], &record, sizeof(record));

    state = hal_flash_erase(imginfo_offset, sizeof(s_imginfo_page));
    if (state == rot_state_success) {
        state = hal_flash_write(imginfo_offset, s_imginfo_page, sizeof(s_imginfo_page));
    }
    if (state == rot_state_success) {
        state = hal_flash_compare(imginfo_offset, s_imginfo_page, sizeof(s_imginfo_page), &equal);
    }
    if ((state == rot_state_success) && !equal) {
        state = rot_state_upgrade_verify_fail;
    }

    return state;
}

static rot_state_t _flush_alignment(uint8_t fill)
{
    rot_state_t state;

    if (s_align_count == 0U) {
        return rot_state_success;
    }

    while (s_align_count < sizeof(s_align_buf)) {
        s_align_buf[s_align_count++] = fill;
    }

    state = hal_flash_write(s_image_offset + s_programmed_size,
                            s_align_buf,
                            sizeof(s_align_buf));
    if (state == rot_state_success) {
        s_programmed_size += sizeof(s_align_buf);
        s_align_count = 0U;
    }
    return state;
}

static rot_state_t _write_image_bytes(const uint8_t *data, uint32_t size)
{
    const uint8_t *cursor = data;
    uint32_t remaining = size;
    rot_state_t state = rot_state_success;

    while ((remaining > 0U) && (s_align_count < sizeof(s_align_buf))) {
        s_align_buf[s_align_count++] = *cursor++;
        remaining--;
    }
    if (s_align_count == sizeof(s_align_buf)) {
        state = _flush_alignment(0U);
        if (state != rot_state_success) {
            return state;
        }
    }

    if (remaining >= sizeof(s_align_buf)) {
        uint32_t bulk_size = remaining & ~3U;

        state = hal_flash_write(s_image_offset + s_programmed_size, cursor, bulk_size);
        if (state != rot_state_success) {
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
    return rot_state_success;
}

rot_state_t upgrade_init(void)
{
    rot_state_t state;
    uint8_t active_slot = 0U;

    if (s_initialized) {
        return rot_state_success;
    }

    state = hal_flash_init();
    if (state != rot_state_success) {
        return state;
    }
    if (hal_flash_get_sector_size() != FDB_PAGE_SIZ) {
        printf(TAG "sector size %u does not match FDB map page size %u\r\n",
               (unsigned)hal_flash_get_sector_size(), (unsigned)FDB_PAGE_SIZ);
        return rot_state_upgrade_init_fail;
    }

    state = hal_flash_get_active_app_slot(&active_slot);
    if (state != rot_state_success) {
        return state;
    }
    if (!_slot_valid(active_slot)) {
        return rot_state_upgrade_init_fail;
    }

    s_active_slot = active_slot;
    s_target_slot = (uint8_t)(active_slot ^ 1U);
    s_state = UPGRADE_STATE_IDLE;
    s_last_result = rot_state_success;
    s_initialized = true;

    printf(TAG "active APP%u, next target APP%u\r\n",
           (unsigned)(s_active_slot + 1U), (unsigned)(s_target_slot + 1U));
    return rot_state_success;
}

rot_state_t upgrade_get_status(upgrade_status_t *status)
{
    if (status == NULL) {
        return rot_state_invalid_argument;
    }
    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }

    status->state = s_state;
    status->active_slot = s_active_slot;
    status->target_slot = s_target_slot;
    status->image_offset = s_image_offset;
    status->image_size = s_image_size;
    status->received_size = s_received_size;
    status->expected_checksum = s_expected_checksum;
    status->checksum = s_checksum;
    status->last_result = s_last_result;
    return rot_state_success;
}

rot_state_t upgrade_get_slot_info(uint8_t slot, uint32_t *image_offset, uint32_t *capacity)
{
    if ((image_offset == NULL) || (capacity == NULL)) {
        return rot_state_invalid_argument;
    }
    if (!_slot_valid(slot)) {
        return rot_state_upgrade_invalid_slot;
    }

    *image_offset = _slot_image_offset(slot);
    *capacity = FDB_APP_IMG_SIZ;
    return rot_state_success;
}

rot_state_t upgrade_get_active_slot(uint8_t *slot)
{
    if (slot == NULL) {
        return rot_state_invalid_argument;
    }
    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }

    *slot = s_active_slot;
    return rot_state_success;
}


rot_state_t upgrade_get_runtime_info(upgrade_runtime_info_t *info)
{
    rot_state_t state;

    if (info == NULL) {
        return rot_state_invalid_argument;
    }
    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }

    memset(info, 0, sizeof(*info));
    info->active_slot = s_active_slot;
    info->remap_enabled = (s_active_slot == 1U) ? 1U : 0U;
    info->firmware_version = ROT_VER;

    uint32_t otp_boot = rom_otp_read_from_shadow(OTP_BOOT_WORD_INDEX);
    info->otp_sec_img_offset =
        (uint16_t)(otp_boot & OTP_SEC_IMG_OFFSET_MASK);
    info->otp_max_img_len =
        (uint16_t)(otp_boot >> OTP_MAX_IMG_LEN_SHIFT);
    info->configured_slot_size_256k =
        (uint16_t)(FDB_APP_IMG_SIZ / (256U * 1024U));
    info->boot_ota_enabled =
        (info->otp_sec_img_offset != 0U) ? 1U : 0U;
    info->boot_layout_matches =
        (info->otp_sec_img_offset == info->configured_slot_size_256k)
        ? 1U : 0U;

    for (uint8_t slot = 0U; slot < FDB_APP_IMG_NUM; slot++) {
        state = _read_slot_version(slot,
                                   &info->slot_version[slot],
                                   &info->slot_boot_header_length[slot],
                                   &info->slot_valid[slot]);
        if (state != rot_state_success) {
            return state;
        }
    }

    info->active_version = info->slot_version[info->active_slot];
    info->active_boot_header_length =
        info->slot_boot_header_length[info->active_slot];
    return rot_state_success;
}


rot_state_t upgrade_begin(const upgrade_image_header_t *header)
{
    rot_state_t state;
    uint32_t erase_size;
    uint8_t target_slot;

    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }
    if (header == NULL) {
        return rot_state_invalid_argument;
    }
    if ((s_state == UPGRADE_STATE_RECEIVING) || (s_state == UPGRADE_STATE_VERIFIED)) {
        return rot_state_upgrade_busy;
    }
    if ((header->magic != UPGRADE_IMAGE_HEADER_MAGIC) ||
        (header->device != UPGRADE_DEVICE_ID) ||
        (header->len == 0U)) {
        return _fail(rot_state_upgrade_invalid_header);
    }
    if (header->len > FDB_APP_IMG_SIZ) {
        return _fail(rot_state_upgrade_image_too_large);
    }

    target_slot = (uint8_t)(s_active_slot ^ 1U);
    erase_size = ROT_ALIGN_UP(header->len, FDB_PAGE_SIZ);
    if (erase_size > FDB_APP_IMG_SIZ) {
        erase_size = FDB_APP_IMG_SIZ;
    }

    s_target_slot = target_slot;
    s_image_offset = _slot_image_offset(target_slot);
    s_image_size = header->len;
    s_received_size = 0U;
    s_programmed_size = 0U;
    s_expected_checksum = header->checksum;
    s_checksum = 0U;
    s_image_crc32 = 0xffffffffUL;
    s_header = *header;
    s_align_count = 0U;
    s_last_result = rot_state_success;

    state = hal_flash_erase(s_image_offset, erase_size);
    if (state != rot_state_success) {
        return _fail(state);
    }

    s_state = UPGRADE_STATE_RECEIVING;
    printf(TAG "receiving APP%u image: 0x%08x, %u byte(s)\r\n",
           (unsigned)(target_slot + 1U),
           (unsigned)(FDB_BASE_ADDR + s_image_offset),
           (unsigned)s_image_size);
    return rot_state_success;
}

rot_state_t upgrade_write(const void *data, uint32_t len)
{
    rot_state_t state;
    const uint8_t *bytes = (const uint8_t *)data;

    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }
    if (s_state == UPGRADE_STATE_FAILED) {
        return s_last_result;
    }
    if (s_state != UPGRADE_STATE_RECEIVING) {
        return rot_state_upgrade_incomplete;
    }
    if ((bytes == NULL) || (len == 0U)) {
        return rot_state_invalid_argument;
    }
    if (len > (s_image_size - s_received_size)) {
        return _fail(rot_state_upgrade_image_too_large);
    }

    state = _write_image_bytes(bytes, len);
    if (state != rot_state_success) {
        return _fail(state);
    }

    for (uint32_t i = 0U; i < len; i++) {
        s_checksum += bytes[i];
    }
    s_image_crc32 = _crc32_update(s_image_crc32, bytes, len);
    s_received_size += len;
    return rot_state_success;
}

rot_state_t upgrade_finish(void)
{
    rot_state_t state;
    uint32_t flash_checksum = 0U;
    uint32_t flash_crc32 = 0U;
    uint16_t sw_version = 0U;
    uint16_t boot_header_length = 0U;

    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }
    if (s_state == UPGRADE_STATE_VERIFIED) {
        return rot_state_success;
    }
    if (s_state == UPGRADE_STATE_FAILED) {
        return s_last_result;
    }
    if (s_state != UPGRADE_STATE_RECEIVING) {
        return rot_state_upgrade_incomplete;
    }
    if (s_received_size != s_image_size) {
        return rot_state_upgrade_incomplete;
    }

    state = _flush_alignment(0xffU);
    if (state != rot_state_success) {
        return _fail(state);
    }

    state = _verify_flash(s_image_offset, s_image_size, &flash_checksum, &flash_crc32);
    if (state != rot_state_success) {
        return _fail(state);
    }
    if (flash_checksum != s_expected_checksum) {
        printf(TAG "checksum mismatch: expected 0x%08x, stored 0x%08x\r\n",
               (unsigned)s_expected_checksum, (unsigned)flash_checksum);
        return _fail(rot_state_upgrade_checksum_mismatch);
    }
    if (flash_crc32 != ~s_image_crc32) {
        return _fail(rot_state_upgrade_checksum_mismatch);
    }

    state = _read_boot_header(s_image_offset, s_image_size, &sw_version, &boot_header_length);
    if (state != rot_state_success) {
        return _fail(state);
    }

    state = _write_record(s_target_slot, s_image_size, flash_checksum, flash_crc32,
                          sw_version, boot_header_length, &s_header);
    if (state != rot_state_success) {
        return _fail(state);
    }

    s_checksum = flash_checksum;
    s_state = UPGRADE_STATE_VERIFIED;
    s_last_result = rot_state_success;
    printf(TAG "APP%u verified, sw_version=%u, checksum=0x%08x, crc=0x%08x\r\n", (unsigned)(s_target_slot + 1U), (unsigned)sw_version, (unsigned)flash_checksum, (unsigned)flash_crc32);
    return rot_state_success;
}

rot_state_t upgrade_feed(const void *data, uint32_t len, bool *image_complete)
{
    rot_state_t state;
    const uint8_t *cursor = (const uint8_t *)data;

    if (image_complete != NULL) {
        *image_complete = false;
    }
    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }
    if ((cursor == NULL) || (len == 0U)) {
        return rot_state_invalid_argument;
    }
    if (s_state == UPGRADE_STATE_FAILED) {
        return s_last_result;
    }
    if (s_state == UPGRADE_STATE_VERIFIED) {
        if (image_complete != NULL) {
            *image_complete = true;
        }
        return rot_state_success;
    }

    if (s_state == UPGRADE_STATE_IDLE) {
        upgrade_image_header_t header;

        if (len < sizeof(header)) {
            return _fail(rot_state_upgrade_invalid_header);
        }

        header.magic = _le32(&cursor[0]);
        header.device = _le32(&cursor[4]);
        header.len = _le32(&cursor[8]);
        header.checksum = _le32(&cursor[12]);

        state = upgrade_begin(&header);
        if (state != rot_state_success) {
            return state;
        }

        cursor += sizeof(header);
        len -= sizeof(header);
        if (len == 0U) {
            return rot_state_success;
        }
    }

    if (s_received_size == s_image_size) {
        if (image_complete != NULL) {
            *image_complete = true;
        }
        return rot_state_success;
    }

    uint32_t remaining = s_image_size - s_received_size;
    uint32_t write_size = (len < remaining) ? len : remaining;

    state = upgrade_write(cursor, write_size);
    if (state != rot_state_success) {
        return state;
    }
    if (image_complete != NULL) {
        *image_complete = (s_received_size == s_image_size);
    }
    return rot_state_success;
}

rot_state_t upgrade_auto_write(const void *data, uint32_t len, bool *image_complete)
{
    return upgrade_feed(data, len, image_complete);
}

rot_state_t upgrade_abort(void)
{
    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }

    s_state = UPGRADE_STATE_IDLE;
    s_target_slot = (uint8_t)(s_active_slot ^ 1U);
    s_image_offset = 0U;
    s_image_size = 0U;
    s_received_size = 0U;
    s_programmed_size = 0U;
    s_expected_checksum = 0U;
    s_checksum = 0U;
    s_image_crc32 = 0U;
    memset(&s_header, 0, sizeof(s_header));
    s_align_count = 0U;
    s_last_result = rot_state_success;
    return rot_state_success;
}

rot_state_t upgrade_read_record(uint8_t slot, upgrade_record_t *record)
{
    uint8_t raw[UPGRADE_RECORD_READ_SIZE] __attribute__((aligned(4)));
    upgrade_image_header_t header;
    rot_state_t state;

    if (record == NULL) {
        return rot_state_invalid_argument;
    }
    if (!s_initialized) {
        return rot_state_upgrade_not_init;
    }
    if (!_slot_valid(slot)) {
        return rot_state_upgrade_invalid_slot;
    }

    state = hal_flash_read(_slot_imginfo_offset(slot), raw, sizeof(raw));
    if (state != rot_state_success) {
        return state;
    }
    header.magic = _le32(&raw[0]);
    header.device = _le32(&raw[4]);
    header.len = _le32(&raw[8]);
    header.checksum = _le32(&raw[12]);
    memcpy(record, &raw[UPGRADE_RECORD_OFFSET], sizeof(*record));

    if ((header.magic != UPGRADE_IMAGE_HEADER_MAGIC) ||
        (header.device != UPGRADE_DEVICE_ID) ||
        (header.len != record->image_size) ||
        (header.checksum != record->image_checksum) ||
        !_record_valid(record, slot)) {
        return rot_state_upgrade_record_invalid;
    }
    return rot_state_success;
}

rot_state_t upgrade_validate_slot(uint8_t slot, upgrade_record_t *record)
{
    upgrade_record_t local_record;
    upgrade_record_t *out = (record == NULL) ? &local_record : record;
    uint32_t checksum = 0U;
    uint32_t crc32 = 0U;
    uint16_t sw_version = 0U;
    uint16_t boot_header_length = 0U;
    rot_state_t state;

    if (!_slot_valid(slot)) {
        return rot_state_upgrade_invalid_slot;
    }

    state = upgrade_read_record(slot, out);
    if (state != rot_state_success) {
        return state;
    }
    state = _verify_flash(out->image_offset, out->image_size, &checksum, &crc32);
    if (state != rot_state_success) {
        return state;
    }
    if (checksum != out->image_checksum) {
        return rot_state_upgrade_checksum_mismatch;
    }
    if (crc32 != out->image_crc32) {
        return rot_state_upgrade_checksum_mismatch;
    }
    state = _read_boot_header(out->image_offset, out->image_size,
                              &sw_version, &boot_header_length);
    if (state != rot_state_success) {
        return state;
    }
    if ((sw_version != out->sw_version) ||
        (boot_header_length != out->boot_header_length)) {
        return rot_state_upgrade_record_invalid;
    }
    return rot_state_success;
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
