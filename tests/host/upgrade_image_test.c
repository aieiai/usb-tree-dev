#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


#define FOTA_HEADER_SIZE        (16U)
#define FOTA_BOOT_HEADER_OFFSET (FOTA_HEADER_SIZE + 0x1000U)
#define FOTA_MAGIC              (0xbeaf5aa5UL)
#define FOTA_DEVICE_ID          (0x6750UL)


static uint16_t le16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}


static uint32_t le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}


static int test_file(const char *path)
{
    FILE *file;
    uint8_t fota_header[FOTA_HEADER_SIZE];
    uint8_t boot_header[16];
    uint32_t payload_length;
    uint32_t expected_checksum;
    uint32_t actual_checksum = 0U;
    uint32_t remaining;
    uint16_t boot_header_length;
    uint16_t sw_version;
    uint8_t buffer[4096];

    file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return 1;
    }
    if (fread(fota_header, 1U, sizeof(fota_header), file) != sizeof(fota_header)) {
        fclose(file);
        return 1;
    }

    assert(le32(&fota_header[0]) == FOTA_MAGIC);
    assert(le32(&fota_header[4]) == FOTA_DEVICE_ID);
    payload_length = le32(&fota_header[8]);
    expected_checksum = le32(&fota_header[12]);

    remaining = payload_length;
    while (remaining > 0U) {
        size_t chunk = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
        if (fread(buffer, 1U, chunk, file) != chunk) {
            fclose(file);
            return 1;
        }
        for (size_t i = 0U; i < chunk; i++) {
            actual_checksum += buffer[i];
        }
        remaining -= (uint32_t)chunk;
    }
    assert(actual_checksum == expected_checksum);

    if ((fseek(file, (long)FOTA_BOOT_HEADER_OFFSET, SEEK_SET) != 0) ||
        (fread(boot_header, 1U, sizeof(boot_header), file) != sizeof(boot_header))) {
        fclose(file);
        return 1;
    }
    fclose(file);

    assert(boot_header[0] == 0xbfU);
    boot_header_length = le16(&boot_header[2]);
    assert(boot_header_length >= sizeof(boot_header));
    assert(boot_header_length <= 0x2000U);
    sw_version = le16(&boot_header[8]);
    assert(sw_version != 0U);

    printf("%s: SW_VERSION=%u, checksum=0x%08x\n",
           path,
           (unsigned)sw_version,
           (unsigned)actual_checksum);
    return 0;
}


int main(int argc, char **argv)
{
    assert(argc > 1);
    for (int i = 1; i < argc; i++) {
        if (test_file(argv[i]) != 0) {
            return 1;
        }
    }
    printf("upgrade image tests passed\n");
    return 0;
}
