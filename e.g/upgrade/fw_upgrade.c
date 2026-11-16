#include "fw_upgrade.h"

#include <string.h>

#include "app_debug.h"
#include "flash_map.h"
#include "gs_hub.h"
#include "hal_flash.h"
#include "hal_gpio.h"
#include "hpm_bootheader.h"
#include "hpm_romapi.h"
#include "ota_api.h"

#define TAG "fw_upgrade: "

#define DATA_SEQ_SIZE       (4u)
#define DATA_MAX            (48u)
#define START_MIN_LEN       (22u)
#define SIZE_OFFSET         (14u)
#define CHECKSUM_OFFSET     (18u)

#define BOOT_TAG            (HPM_BOOTHEADER_TAG)
#define BOOT_VER            (0x10u)
#define FLASH_BOOT_OFFSET   (FLASH_APP1_BOOT_HEAD_ADDR - FLASH_ADDR_BASE)


typedef struct {
    bool active;
    bool last_valid;
    uint8_t cur_slot;
    uint8_t encrypted;
    const upgrade_slot_t *target;
    uint16_t active_version;

    uint32_t image_size;
    uint32_t image_checksum;
    uint32_t received_size;
    uint32_t received_checksum;
    uint32_t image_base_offset;
    uint32_t file_boot_offset;

    uint32_t next_seq;
    uint32_t last_seq;
    uint16_t last_len;
    uint8_t last_data[DATA_MAX];

    uint16_t boot_collected;
    boot_header_t image_boot;
} upgrade_ctx_t;

const upgrade_slot_t upgrade_slot[] = {
    {
        .index = 0u,
        .app_offset = FLASH_APP1_ALL_IMG_ADDR - FLASH_ADDR_BASE,
        .app_size = FLASH_APP1_ALL_IMG_SIZE,
        .info_offset = FLASH_APP1_IMGINFO_ADDR - FLASH_ADDR_BASE,
        .info_size = FLASH_APP1_IMGINFO_SIZE,
    },
    {
        .index = 1u,
        .app_offset = FLASH_APP2_ALL_IMG_ADDR - FLASH_ADDR_BASE,
        .app_size = FLASH_APP2_ALL_IMG_SIZE,
        .info_offset = FLASH_APP2_IMGINFO_ADDR - FLASH_ADDR_BASE,
        .info_size = FLASH_APP2_IMGINFO_SIZE,
    },
};

static upgrade_ctx_t s_ctx;
static uint32_t s_chunk_buf[DATA_MAX / sizeof(uint32_t)];
static uint32_t s_sector_buf[FLASH_SECTOR_SIZE / sizeof(uint32_t)];

_Static_assert(sizeof(boot_header_t) == 16u, "boot_header_t must be 16 bytes");
_Static_assert(DATA_MAX <= sizeof(s_chunk_buf), "chunk buffer too small");


static uint32_t slot_boot_offset(const upgrade_slot_t *slot)
{
    return slot->app_offset + FLASH_BOOT_OFFSET;
}

static bool boot_header_valid(const boot_header_t *header)
{
    return (header->tag == BOOT_TAG) && (header->version == BOOT_VER)
        && (header->length >= sizeof(boot_header_t)) && (header->length <= FLASH_APP1_BOOT_HEAD_SIZE)
        && (header->fw_count >= 1u) && (header->fw_count <= HPM_BOOTHEADER_MAX_FW_COUNT);
}

static uint32_t file_boot_head_offset(bool encrypted)
{
    return encrypted ? (FLASH_APP1_EXIP_INFO_SIZE + FLASH_APP1_NOR_CFG_SIZE) : FLASH_APP1_NOR_CFG_SIZE;
}

bool upgrade_is_active(void)
{
    return s_ctx.active;
}

uint32_t upgrade_received(void)
{
    return s_ctx.received_size;
}

uint32_t upgrade_image_size(void)
{
    return s_ctx.image_size;
}

static uint32_t rd32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t checksum_add(uint32_t sum, const uint8_t *p, uint32_t len)
{
    while (len-- != 0u)
    {
        sum += *p++;
    }
    return sum;
}

static gs_ec_t read_otp_boot_layout(uint32_t *offset, uint32_t *size)
{
    uint32_t otp_boot = rom_otp_read_from_shadow(25u);
    uint32_t sec_img_offset = otp_boot & 0xFFFFu;
    uint32_t max_img_len = otp_boot >> 16;
    uint32_t unit = FW_OTP_SEC_IMG2_UNIT;

    if (sec_img_offset == 0u)
    {
        return GS_EC_HUB_PARAM;
    }
    *offset = sec_img_offset * unit;
    *size = ((max_img_len != 0u) ? max_img_len : sec_img_offset) * unit;
    return GS_EC_SUCCESS;
}

static gs_ec_t read_slot_version(const upgrade_slot_t *slot, uint16_t *version)
{
    boot_header_t header;
    uint32_t offset = slot_boot_offset(slot);

    if (hal_flash_read(offset, &header, sizeof(header)) != GS_EC_SUCCESS)
    {
        return GS_EC_HUB_FLASH;
    }
    if (!boot_header_valid(&header))
    {
        return GS_EC_HUB_PARAM;
    }
    *version = header.sw_version;
    return GS_EC_SUCCESS;
}

static gs_ec_t erase_slot(const upgrade_slot_t *slot)
{
    uint32_t sector_size = hal_flash_get_sector_size();

    if ((sector_size == 0u) || ((slot->app_offset % sector_size) != 0u) || ((slot->app_offset + slot->app_size) > hal_flash_get_chip_size()))
    {
        return GS_EC_HUB_FLASH;
    }
    if (hal_flash_erase(slot->app_offset, slot->app_size) != GS_EC_SUCCESS)
    {
        return GS_EC_HUB_FLASH;
    }
    return GS_EC_SUCCESS;
}

static gs_ec_t fail_upgrade(gs_ec_t error)
{
    gs_ec_t cleanup = GS_EC_SUCCESS;

    if (s_ctx.active && (s_ctx.target != NULL))
    {
        cleanup = erase_slot(s_ctx.target);
    }
    memset(&s_ctx, 0, sizeof(s_ctx));
    return (cleanup == GS_EC_SUCCESS) ? error : cleanup;
}

static void capture_boot_header(uint32_t image_offset, const uint8_t *data, uint16_t len)
{
    uint32_t start = image_offset;
    uint32_t end = image_offset + len;
    uint32_t head = s_ctx.file_boot_offset;
    uint32_t head_end = head + sizeof(boot_header_t);

    if ((end <= head) || (start >= head_end))
    {
        return;
    }
    start = (start > head) ? start : head;
    end = (end < head_end) ? end : head_end;

    memcpy((uint8_t *)&s_ctx.image_boot + (start - head), &data[start - image_offset], end - start);
    if ((end - head) > s_ctx.boot_collected)
    {
        s_ctx.boot_collected = (uint16_t)(end - head);
    }
}

static gs_ec_t write_image_chunk(uint32_t image_offset, const uint8_t *data, uint16_t len)
{
    uint32_t dst = s_ctx.target->app_offset + s_ctx.image_base_offset + image_offset;
    uint32_t dst_end = dst + len;
    uint32_t head = slot_boot_offset(s_ctx.target);
    uint32_t head_end = head + sizeof(boot_header_t);
    uint8_t *buf = (uint8_t *)s_chunk_buf;

    memcpy(s_chunk_buf, data, len);
    if ((dst_end <= head) || (dst >= head_end))
    {
        return hal_flash_write(dst, s_chunk_buf, len);
    }

    if (dst < head)
    {
        uint16_t prefix = (uint16_t)(head - dst);
        if (hal_flash_write(dst, s_chunk_buf, prefix) != GS_EC_SUCCESS)
        {
            return GS_EC_HUB_FLASH;
        }
    }
    if (dst_end > head_end)
    {
        uint16_t skip = (uint16_t)(head_end - dst);
        uint16_t suffix = (uint16_t)(dst_end - head_end);
        memmove(buf, data + skip, suffix);
        if (hal_flash_write(head_end, s_chunk_buf, suffix) != GS_EC_SUCCESS)
        {
            return GS_EC_HUB_FLASH;
        }
    }
    return GS_EC_SUCCESS;
}

static gs_ec_t blank_flash_checksum(uint32_t *checksum)
{
    uint8_t *buf = (uint8_t *)s_chunk_buf;
    uint32_t sum = 0u;
    uint32_t offset = 0u;

    while (offset < s_ctx.image_size)
    {
        uint32_t len = s_ctx.image_size - offset;
        if (len > DATA_MAX)
        {
            len = DATA_MAX;
        }
        if (hal_flash_read(s_ctx.target->app_offset + s_ctx.image_base_offset + offset, s_chunk_buf, len) != GS_EC_SUCCESS)
        {
            return GS_EC_HUB_FLASH;
        }
        sum = checksum_add(sum, buf, len);
        offset += len;
    }
    *checksum = sum;
    return GS_EC_SUCCESS;
}

static gs_ec_t commit_boot_header(void)
{
    uint32_t offset = slot_boot_offset(s_ctx.target);
    boot_header_t verify;

    if (hal_flash_write(offset, &s_ctx.image_boot, sizeof(s_ctx.image_boot)) != GS_EC_SUCCESS)
    {
        return GS_EC_HUB_FLASH;
    }
    if (hal_flash_read(offset, &verify, sizeof(verify)) != GS_EC_SUCCESS)
    {
        return GS_EC_HUB_FLASH;
    }
    return (memcmp(&verify, &s_ctx.image_boot, sizeof(verify)) == 0) ? GS_EC_SUCCESS : GS_EC_HUB_FLASH;
}

static gs_ec_t write_active_version_zero(void)
{
    const upgrade_slot_t *active = &upgrade_slot[s_ctx.cur_slot];
    uint32_t sector_size = hal_flash_get_sector_size();
    uint32_t head = slot_boot_offset(active);
    uint32_t sector;
    uint32_t version_offset;

    if ((sector_size == 0u) || (sector_size > sizeof(s_sector_buf)))
    {
        return GS_EC_HUB_FLASH;
    }
    sector = head - (head % sector_size);
    version_offset = head - sector + FW_HEADER_SW_VERSION_OFFSET;
    if (((version_offset + 2u) > sector_size) || (hal_flash_read(sector, s_sector_buf, sector_size) != GS_EC_SUCCESS))
    {
        return GS_EC_HUB_FLASH;
    }

    ((uint8_t *)s_sector_buf)[version_offset] = 0u;
    ((uint8_t *)s_sector_buf)[version_offset + 1u] = 0u;
    if ((hal_flash_erase(sector, sector_size) != GS_EC_SUCCESS) || (hal_flash_write(sector, s_sector_buf, sector_size) != GS_EC_SUCCESS))
    {
        return GS_EC_HUB_FLASH;
    }
    s_ctx.active_version = 0u;
    return GS_EC_SUCCESS;
}

gs_ec_t upgrade_start(const uint8_t *data, uint16_t len)
{
    uint32_t image_end;
    uint32_t otp_offset;
    uint32_t otp_size;
    gs_ec_t ec;

    if ((data == NULL) || (len < START_MIN_LEN))
    {
        return GS_EC_HUB_LEN;
    }
    if (s_ctx.active)
    {
        return GS_EC_HUB_UPGRADE_STATE;
    }
    if ((read_otp_boot_layout(&otp_offset, &otp_size) != GS_EC_SUCCESS)
        || (otp_offset != (FLASH_APP2_ALL_IMG_ADDR - FLASH_ADDR_BASE))
        || (otp_size != FW_APP_IMG_SIZE))
    {
        return GS_EC_HUB_UPGRADE_SIZE;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.image_size = rd32_le(data + SIZE_OFFSET);
    s_ctx.image_checksum = rd32_le(data + CHECKSUM_OFFSET);
    s_ctx.encrypted = ota_get_exip_enable() ? 1u : 0u;
    s_ctx.cur_slot = ota_get_remap_enable() ? 1u : 0u;
    s_ctx.target = &upgrade_slot[s_ctx.cur_slot ^ 1u];
    s_ctx.image_base_offset = s_ctx.encrypted ? 0u : FLASH_APP1_EXIP_INFO_SIZE;
    s_ctx.file_boot_offset = file_boot_head_offset(s_ctx.encrypted != 0u);

    image_end = s_ctx.target->app_offset + s_ctx.image_base_offset + s_ctx.image_size;
    if ((s_ctx.image_size == 0u)
        || (s_ctx.image_base_offset > s_ctx.target->app_size)
        || (s_ctx.image_size > (s_ctx.target->app_size - s_ctx.image_base_offset))
        || ((s_ctx.file_boot_offset + sizeof(boot_header_t)) > s_ctx.image_size)
        || (image_end > hal_flash_get_chip_size()))
    {
        return GS_EC_HUB_UPGRADE_SIZE;
    }

    ec = read_slot_version(&upgrade_slot[s_ctx.cur_slot], &s_ctx.active_version);
    if (ec != GS_EC_SUCCESS)
    {
        return ec;
    }

    s_ctx.active = true;
    ec = erase_slot(s_ctx.target);
    if (ec != GS_EC_SUCCESS)
    {
        return fail_upgrade(ec);
    }

    GS_LOG_DBG(TAG "APP%u->APP%u size=%u encrypted=%u base=0x%X boot=0x%X\r\n",
               (unsigned)(s_ctx.cur_slot + 1u), (unsigned)(s_ctx.target->index + 1u), (unsigned)s_ctx.image_size,
               (unsigned)s_ctx.encrypted, (unsigned)s_ctx.image_base_offset, (unsigned)s_ctx.file_boot_offset);
    return GS_EC_SUCCESS;
}

gs_ec_t upgrade_write(const uint8_t *data, uint16_t len, uint32_t *seq)
{
    const uint8_t *image;
    uint16_t image_len;
    uint32_t current_seq;
    uint32_t end_size;

    if (!s_ctx.active)
    {
        return GS_EC_HUB_UPGRADE_STATE;
    }
    if ((data == NULL) || (len <= DATA_SEQ_SIZE))
    {
        return GS_EC_HUB_LEN;
    }

    current_seq = rd32_le(data);
    image_len = (uint16_t)(len - DATA_SEQ_SIZE);
    image = data + DATA_SEQ_SIZE;
    if (seq != NULL)
    {
        *seq = current_seq;
    }
    if ((image_len == 0u) || (image_len > DATA_MAX))
    {
        return GS_EC_HUB_LEN;
    }

    if (s_ctx.last_valid && (current_seq == s_ctx.last_seq))
    {
        return ((image_len == s_ctx.last_len) && (memcmp(image, s_ctx.last_data, image_len) == 0)) ? GS_EC_SUCCESS : GS_EC_HUB_UPGRADE_SEQ;
    }
    if (current_seq != s_ctx.next_seq)
    {
        return GS_EC_HUB_UPGRADE_SEQ;
    }

    end_size = s_ctx.received_size + image_len;
    if (end_size > s_ctx.image_size)
    {
        return GS_EC_HUB_UPGRADE_SIZE;
    }

    capture_boot_header(s_ctx.received_size, image, image_len);
    if (write_image_chunk(s_ctx.received_size, image, image_len) != GS_EC_SUCCESS)
    {
        return fail_upgrade(GS_EC_HUB_FLASH);
    }

    s_ctx.received_checksum = checksum_add(s_ctx.received_checksum, image, image_len);
    memcpy(s_ctx.last_data, image, image_len);
    s_ctx.last_len = image_len;
    s_ctx.last_seq = current_seq;
    s_ctx.last_valid = true;
    s_ctx.received_size = end_size;
    s_ctx.next_seq++;
    return GS_EC_SUCCESS;
}

gs_ec_t upgrade_finish(const uint8_t *data, uint16_t len)
{
    uint32_t flash_sum;
    uint32_t header_sum;
    uint32_t erased_sum;
    gs_ec_t ec;

    (void)data;
    if (len < 2u)
    {
        return fail_upgrade(GS_EC_HUB_LEN);
    }
    if (!s_ctx.active)
    {
        return fail_upgrade(GS_EC_HUB_UPGRADE_STATE);
    }
    if ((s_ctx.received_size != s_ctx.image_size)
        || (s_ctx.received_checksum != s_ctx.image_checksum)
        || (s_ctx.boot_collected != sizeof(boot_header_t))
        || !boot_header_valid(&s_ctx.image_boot))
    {
        return fail_upgrade(GS_EC_HUB_UPGRADE_CHECKSUM);
    }

    ec = blank_flash_checksum(&flash_sum);
    header_sum = checksum_add(0u, (const uint8_t *)&s_ctx.image_boot, sizeof(s_ctx.image_boot));
    erased_sum = (uint32_t)hal_flash_get_erase_value() * sizeof(s_ctx.image_boot);
    if ((ec != GS_EC_SUCCESS) || (flash_sum != (s_ctx.image_checksum - header_sum + erased_sum)))
    {
        return fail_upgrade(GS_EC_HUB_UPGRADE_CHECKSUM);
    }

    ec = commit_boot_header();
    if ((ec != GS_EC_SUCCESS) || ((s_ctx.image_boot.sw_version < s_ctx.active_version) && (write_active_version_zero() != GS_EC_SUCCESS)))
    {
        return fail_upgrade(GS_EC_HUB_FLASH);
    }

    GS_LOG_DBG(TAG "verified size=%u checksum=0x%08X version=%u\r\n", (unsigned)s_ctx.image_size, (unsigned)s_ctx.image_checksum, (unsigned)s_ctx.image_boot.sw_version);
    memset(&s_ctx, 0, sizeof(s_ctx));

    return GS_EC_SUCCESS;
}

gs_ec_t upgrade_abort(const uint8_t *data, uint16_t len)
{
    (void)data;
    return (len < 2u) ? fail_upgrade(GS_EC_HUB_LEN) : fail_upgrade(GS_EC_SUCCESS);
}

