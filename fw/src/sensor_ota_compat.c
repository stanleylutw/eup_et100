#include "sensor_ota_compat.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int device_id_to_bytes(const char device_id[EUP_GATT_DEVICE_ID_SIZE], uint8_t out[6])
{
    for (size_t i = 0; i < 6; i++) {
        int hi = hex_nibble(device_id[i * 2]), lo = hex_nibble(device_id[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -EINVAL;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return device_id[12] == '\0' ? 0 : -EINVAL;
}

/* Fixed project + hardware + SoC mapping; never guessed from a board name. */
static const char *mapped_board(const struct eup_gatt_metadata *m)
{
    if (!strcmp(m->project_name, "EUP_AIoT_P1") && !strcmp(m->hardware_version, "P1_HW_1.0.0") &&
        !strcmp(m->soc, "nrf52832"))
        return "p1_nrf52832";
    if (!strcmp(m->project_name, "EUP_AIoT_E9") && !strcmp(m->hardware_version, "E9_HW_1.0.0") &&
        !strcmp(m->soc, "nrf52832"))
        return "e9_nrf52832";
    return NULL;
}

const char *sensor_ota_mismatch_field_name(enum sensor_ota_mismatch_field field)
{
    switch (field) {
    case SENSOR_OTA_MISMATCH_DEVICE_ID: return "DEVICE_ID";
    case SENSOR_OTA_MISMATCH_PROJECT: return "PROJECT";
    case SENSOR_OTA_MISMATCH_HARDWARE: return "HARDWARE";
    case SENSOR_OTA_MISMATCH_SOC: return "SOC";
    case SENSOR_OTA_MISMATCH_BOARD: return "BOARD";
    case SENSOR_OTA_MISMATCH_CAPABILITIES: return "CAPABILITIES";
    case SENSOR_OTA_MISMATCH_SCHEMA: return "SCHEMA";
    case SENSOR_OTA_MISMATCH_PROTOCOL: return "PROTOCOL";
    case SENSOR_OTA_MISMATCH_ROLE: return "ROLE";
    default: return "NONE";
    }
}

int sensor_ota_compat_package_supported(const struct sensor_pkg_info *p)
{
    int product = !strcmp(p->product, "EUP_AIoT_P1") || !strcmp(p->product, "EUP_AIoT_E9");
    int board = !strcmp(p->board, "p1_nrf52832") || !strcmp(p->board, "e9_nrf52832");
    return product && board && !strcmp(p->soc, "nrf52832");
}

int sensor_ota_compat_evaluate(const struct sensor_pkg_info *package,
                               const struct eup_gatt_metadata *metadata,
                               const uint8_t target_mac[6], struct sensor_ota_compat_result *result)
{
    const uint32_t required_valid = EUP_GATT_META_PROJECT | EUP_GATT_META_HARDWARE |
        EUP_GATT_META_FIRMWARE | EUP_GATT_META_BUILD_ID | EUP_GATT_META_SOC |
        EUP_GATT_META_SCHEMA | EUP_GATT_META_PROTOCOL | EUP_GATT_META_DEVICE_ID |
        EUP_GATT_META_CAPABILITIES | EUP_GATT_META_AIOT_ROLE;
    const uint32_t required_commands = EUP_GATT_CMD_SUCCESS(0x01u) | EUP_GATT_CMD_SUCCESS(0x05u) |
        EUP_GATT_CMD_SUCCESS(0x07u) | EUP_GATT_CMD_SUCCESS(0x08u);
    uint8_t device_id[6];

    if (!package || !metadata || !target_mac || !result) return -EINVAL;
    memset(result, 0, sizeof *result);
    const char *board = mapped_board(metadata);
    if (board) snprintf(result->board, sizeof result->board, "%s", board);
    if ((metadata->valid_mask & required_valid) != required_valid ||
        (metadata->command_success_mask & required_commands) != required_commands)
        return -ENODATA;
    if (device_id_to_bytes(metadata->device_id, device_id) || memcmp(device_id, target_mac, 6)) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_DEVICE_ID;
        return -EXDEV;
    }
    result->match_mask |= SENSOR_OTA_MATCH_DEVICE_ID;
    if (strcmp(metadata->project_name, package->product)) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_PROJECT;
        return -EXDEV;
    }
    result->match_mask |= SENSOR_OTA_MATCH_PROJECT;
    if (strcmp(metadata->hardware_version, package->hardware)) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_HARDWARE;
        return -EXDEV;
    }
    result->match_mask |= SENSOR_OTA_MATCH_HARDWARE;
    if (strcmp(metadata->soc, package->soc)) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_SOC;
        return -EXDEV;
    }
    result->match_mask |= SENSOR_OTA_MATCH_SOC;
    if (!board || strcmp(board, package->board)) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_BOARD;
        return -EXDEV;
    }
    result->match_mask |= SENSOR_OTA_MATCH_BOARD;
    if ((metadata->capabilities & EUP_AIOT_REQUIRED_CAPS) != EUP_AIOT_REQUIRED_CAPS) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_CAPABILITIES;
        return -ENOTSUP;
    }
    result->match_mask |= SENSOR_OTA_MATCH_CAPABILITIES;
    if (metadata->metadata_schema != 1u) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_SCHEMA;
        return -EPROTONOSUPPORT;
    }
    result->match_mask |= SENSOR_OTA_MATCH_SCHEMA;
    if (metadata->protocol_version != 1u) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_PROTOCOL;
        return -EPROTONOSUPPORT;
    }
    result->match_mask |= SENSOR_OTA_MATCH_PROTOCOL;
    if (metadata->aiot_role != 0u) {
        result->mismatch_field = SENSOR_OTA_MISMATCH_ROLE;
        return -EXDEV;
    }
    if (!strcmp(metadata->fw_version, package->firmware)) result->match_mask |= SENSOR_OTA_MATCH_FIRMWARE;
    return 0;
}
