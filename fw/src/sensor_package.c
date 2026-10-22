/* Port of eup_gateway src/ota/sensor_ota_package.c (Apache-2.0): same checks,
 * tinycrypt/Zephyr CRC replaced by mbedtls SHA-256 and the IEEE CRC-32 already
 * used for EUPFWV1. */
#include "sensor_package.h"
#include "integrity.h"
#include "mbedtls/sha256.h"
#include <stdbool.h>
#include <string.h>

#define MCUBOOT_IMAGE_MAGIC 0x96f3b83du
#define MCUBOOT_TLV_INFO_MAGIC 0x6907u
#define MCUBOOT_TLV_PROT_MAGIC 0x6908u
#define MCUBOOT_TLV_SHA256 0x10u

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static bool fixed_string_valid(const uint8_t *field, size_t size)
{
    return field[0] != '\0' && memchr(field, '\0', size) != NULL;
}

static int hash_image(sensor_pkg_read_fn read, void *ctx, uint32_t length, uint8_t digest[32])
{
    uint8_t block[256];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    int rc = mbedtls_sha256_starts_ret(&sha, 0);
    for (uint32_t off = 0; !rc && off < length;) {
        size_t n = length - off < sizeof block ? length - off : sizeof block;
        if (read(ctx, SENSOR_PKG_HEADER_SIZE + off, block, n)) { rc = -1; break; }
        rc = mbedtls_sha256_update_ret(&sha, block, n);
        off += (uint32_t)n;
    }
    if (!rc) rc = mbedtls_sha256_finish_ret(&sha, digest);
    mbedtls_sha256_free(&sha);
    return rc ? SENSOR_PKG_EIO : 0;
}

static int find_mcuboot_hash(sensor_pkg_read_fn read, void *ctx, uint32_t image_length, uint8_t digest[32])
{
    uint8_t header[32], info[4], tlv[4];
    bool found = false;
    if (image_length < sizeof header || read(ctx, SENSOR_PKG_HEADER_SIZE, header, sizeof header) ||
        get32(header) != MCUBOOT_IMAGE_MAGIC)
        return SENSOR_PKG_EBADMSG;
    uint32_t header_size = get16(header + 8), protected_size = get16(header + 10);
    uint32_t image_size = get32(header + 12);
    if (header_size < sizeof header || header_size > image_length ||
        image_size > image_length - header_size ||
        protected_size > image_length - header_size - image_size)
        return SENSOR_PKG_EBADMSG;
    uint32_t offset = header_size + image_size;
    if (protected_size) {
        if (protected_size < sizeof info ||
            read(ctx, SENSOR_PKG_HEADER_SIZE + offset, info, sizeof info) ||
            get16(info) != MCUBOOT_TLV_PROT_MAGIC || get16(info + 2) != protected_size)
            return SENSOR_PKG_EBADMSG;
        offset += protected_size;
    }
    if (offset + sizeof info > image_length ||
        read(ctx, SENSOR_PKG_HEADER_SIZE + offset, info, sizeof info) ||
        get16(info) != MCUBOOT_TLV_INFO_MAGIC)
        return SENSOR_PKG_EBADMSG;
    uint32_t end = offset + get16(info + 2);
    if (end > image_length || end < offset + sizeof info) return SENSOR_PKG_EBADMSG;
    offset += sizeof info;
    while (offset < end) {
        if (end - offset < sizeof tlv || read(ctx, SENSOR_PKG_HEADER_SIZE + offset, tlv, sizeof tlv))
            return SENSOR_PKG_EBADMSG;
        uint16_t length = get16(tlv + 2);
        offset += sizeof tlv;
        if (length > end - offset) return SENSOR_PKG_EBADMSG;
        if (tlv[0] == MCUBOOT_TLV_SHA256) {
            if (found || length != 32 || read(ctx, SENSOR_PKG_HEADER_SIZE + offset, digest, 32))
                return SENSOR_PKG_EBADMSG;
            found = true;
        }
        offset += length;
    }
    return found ? 0 : SENSOR_PKG_EBADMSG;
}

int sensor_pkg_parse_header(const uint8_t header[SENSOR_PKG_HEADER_SIZE], uint32_t package_length,
                            struct sensor_pkg_info *info)
{
    static const uint8_t magic[8] = { 'E', 'U', 'P', 'S', 'R', 'L', 'Y', '1' };
    if (!header || !info || package_length < SENSOR_PKG_HEADER_SIZE ||
        package_length > SENSOR_PKG_MAX_SIZE)
        return SENSOR_PKG_EINVAL;
    if (memcmp(header, magic, sizeof magic) || get16(header + 0x08) != 1 ||
        get16(header + 0x0a) != SENSOR_PKG_HEADER_SIZE || get32(header + 0x0c) != package_length ||
        get32(header + 0x10) != SENSOR_PKG_HEADER_SIZE || get32(header + 0x18) != 0 ||
        installer_crc32(header, 0xfc) != get32(header + 0xfc))
        return SENSOR_PKG_EBADMSG;
    uint32_t image_length = get32(header + 0x14);
    if (!image_length || image_length > SENSOR_PKG_IMAGE_MAX ||
        package_length != SENSOR_PKG_HEADER_SIZE + image_length ||
        !fixed_string_valid(header + 0x1c, 24) || !fixed_string_valid(header + 0x34, 20) ||
        !fixed_string_valid(header + 0x48, 32) || !fixed_string_valid(header + 0x68, 16) ||
        !fixed_string_valid(header + 0x78, 24))
        return SENSOR_PKG_EBADMSG;
    if (header[0xd0] && !fixed_string_valid(header + 0xd0, 16)) return SENSOR_PKG_EBADMSG;
    for (size_t i = header[0xd0] ? 0xe0u : 0xd0u; i < 0xfcu; i++)
        if (header[i]) return SENSOR_PKG_EBADMSG;
    memset(info, 0, sizeof *info);
    info->package_length = package_length;
    info->image_length = image_length;
    memcpy(info->product, header + 0x1c, sizeof info->product);
    memcpy(info->hardware, header + 0x34, sizeof info->hardware);
    memcpy(info->firmware, header + 0x48, sizeof info->firmware);
    memcpy(info->soc, header + 0x68, sizeof info->soc);
    memcpy(info->board, header + 0x78, sizeof info->board);
    memcpy(info->build_id, header + 0xd0, sizeof info->build_id);
    memcpy(info->upload_sha256, header + 0x90, 32);
    memcpy(info->mcuboot_image_hash, header + 0xb0, 32);
    return 0;
}

int sensor_pkg_validate(sensor_pkg_read_fn read, void *context, uint32_t package_length,
                        struct sensor_pkg_info *info)
{
    uint8_t header[SENSOR_PKG_HEADER_SIZE], digest[32], image_hash[32];
    if (!read || !info || read(context, 0, header, sizeof header)) return SENSOR_PKG_EINVAL;
    int rc = sensor_pkg_parse_header(header, package_length, info);
    if (rc) return rc;
    if (hash_image(read, context, info->image_length, digest) ||
        memcmp(digest, header + 0x90, 32) ||
        find_mcuboot_hash(read, context, info->image_length, image_hash) ||
        memcmp(image_hash, header + 0xb0, 32))
        return SENSOR_PKG_EBADMSG;
    return 0;
}
