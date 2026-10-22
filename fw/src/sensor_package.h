#ifndef ET100_SENSOR_PACKAGE_H
#define ET100_SENSOR_PACKAGE_H
/* v0.8.8: Gateway Relay Header v1 sensor package (256 B header + original sensor
 * signed image), byte-for-byte the format of eup_gateway include/sensor_ota_package.h
 * and src/ota/sensor_ota_package.c. Validation is ported, not redesigned. */
#include <stddef.h>
#include <stdint.h>

#define SENSOR_PKG_HEADER_SIZE 256u
#define SENSOR_PKG_MAX_SIZE 225280u
#define SENSOR_PKG_IMAGE_MAX (SENSOR_PKG_MAX_SIZE - SENSOR_PKG_HEADER_SIZE)

enum sensor_pkg_error { SENSOR_PKG_EINVAL = -1, SENSOR_PKG_EBADMSG = -2, SENSOR_PKG_EIO = -3 };

struct sensor_pkg_info {
    uint32_t package_length, image_length;
    char product[24], hardware[20], firmware[32], soc[16], board[24], build_id[16];
    uint8_t upload_sha256[32], mcuboot_image_hash[32];
};
/* Reads package bytes at offset (0 = start of the 256 B header). */
typedef int (*sensor_pkg_read_fn)(void *context, uint32_t offset, void *data, size_t length);

int sensor_pkg_parse_header(const uint8_t header[SENSOR_PKG_HEADER_SIZE], uint32_t package_length,
                            struct sensor_pkg_info *info);
/* Header, image SHA-256 (header 0x90) and MCUboot SHA-256 TLV (header 0xB0). */
int sensor_pkg_validate(sensor_pkg_read_fn read, void *context, uint32_t package_length,
                        struct sensor_pkg_info *info);
#endif
