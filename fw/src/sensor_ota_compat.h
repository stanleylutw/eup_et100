#ifndef ET100_SENSOR_OTA_COMPAT_H
#define ET100_SENSOR_OTA_COMPAT_H
/* v0.8.9: package ↔ target Sensor compatibility, ported unchanged from
 * eup_gateway src/ota/sensor_ota_compat.c (UUID metadata + fixed board map). */
#include "sensor_package.h"
#include "eup_aiot_metadata.h"
#include <stdint.h>

enum sensor_ota_mismatch_field {
    SENSOR_OTA_MISMATCH_NONE = 0,
    SENSOR_OTA_MISMATCH_DEVICE_ID,
    SENSOR_OTA_MISMATCH_PROJECT,
    SENSOR_OTA_MISMATCH_HARDWARE,
    SENSOR_OTA_MISMATCH_SOC,
    SENSOR_OTA_MISMATCH_BOARD,
    SENSOR_OTA_MISMATCH_CAPABILITIES,
    SENSOR_OTA_MISMATCH_SCHEMA,
    SENSOR_OTA_MISMATCH_PROTOCOL,
    SENSOR_OTA_MISMATCH_ROLE,
};

#define SENSOR_OTA_MATCH_DEVICE_ID    (1ul << 0)
#define SENSOR_OTA_MATCH_PROJECT      (1ul << 1)
#define SENSOR_OTA_MATCH_HARDWARE     (1ul << 2)
#define SENSOR_OTA_MATCH_SOC          (1ul << 3)
#define SENSOR_OTA_MATCH_BOARD        (1ul << 4)
#define SENSOR_OTA_MATCH_CAPABILITIES (1ul << 5)
#define SENSOR_OTA_MATCH_SCHEMA       (1ul << 6)
#define SENSOR_OTA_MATCH_PROTOCOL     (1ul << 7)
#define SENSOR_OTA_MATCH_FIRMWARE     (1ul << 8)
#define SENSOR_OTA_MATCH_ACTIVE_HASH  (1ul << 9)

struct sensor_ota_compat_result {
    char board[24];
    uint32_t match_mask;
    enum sensor_ota_mismatch_field mismatch_field;
};

/* 0 compatible; -ENODATA required metadata missing; -EXDEV target mismatch;
 * -ENOTSUP capability missing; -EPROTONOSUPPORT schema/protocol; -EINVAL args. */
int sensor_ota_compat_evaluate(const struct sensor_pkg_info *package,
                               const struct eup_gatt_metadata *metadata,
                               const uint8_t target_mac[6], struct sensor_ota_compat_result *result);
const char *sensor_ota_mismatch_field_name(enum sensor_ota_mismatch_field field);
/* Boards the job may update (gateway package_supported). */
int sensor_ota_compat_package_supported(const struct sensor_pkg_info *package);
#endif
