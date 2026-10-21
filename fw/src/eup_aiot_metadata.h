/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EUP_AIOT_METADATA_H_
#define EUP_AIOT_METADATA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EUP_GATT_PROJECT_SIZE    25U
#define EUP_GATT_HARDWARE_SIZE   21U
#define EUP_GATT_FIRMWARE_SIZE   33U
#define EUP_GATT_BUILD_TIME_SIZE 21U
#define EUP_GATT_BUILD_ID_SIZE   17U
#define EUP_GATT_BLE_NAME_SIZE   25U
#define EUP_GATT_SOC_SIZE        17U
#define EUP_GATT_DEVICE_ID_SIZE  13U

#define EUP_GATT_META_PROJECT      (1UL << 0)
#define EUP_GATT_META_HARDWARE     (1UL << 1)
#define EUP_GATT_META_FIRMWARE     (1UL << 2)
#define EUP_GATT_META_BUILD_TIME   (1UL << 3)
#define EUP_GATT_META_BUILD_ID     (1UL << 4)
#define EUP_GATT_META_BLE_NORMAL   (1UL << 5)
#define EUP_GATT_META_BLE_OTA      (1UL << 6)
#define EUP_GATT_META_SOC          (1UL << 7)
#define EUP_GATT_META_SCHEMA       (1UL << 8)
#define EUP_GATT_META_PROTOCOL     (1UL << 9)
#define EUP_GATT_META_DEVICE_ID    (1UL << 10)
#define EUP_GATT_META_CAPABILITIES (1UL << 11)
#define EUP_GATT_META_AIOT_ROLE    (1UL << 12)

#define EUP_GATT_CMD_SUCCESS(cmd) (1UL << ((cmd)-1U))

#define EUP_AIOT_CAP_FW_INFO   (1UL << 0)
#define EUP_AIOT_CAP_OTA_READY (1UL << 1)
#define EUP_AIOT_CAP_DEVICE_ID (1UL << 5)
#define EUP_AIOT_CAP_AIOT_ROLE (1UL << 7)
#define EUP_AIOT_CAP_OTA_AUTH  (1UL << 8)
#define EUP_AIOT_REQUIRED_CAPS                                                                     \
	(EUP_AIOT_CAP_FW_INFO | EUP_AIOT_CAP_OTA_READY | EUP_AIOT_CAP_DEVICE_ID |                  \
	 EUP_AIOT_CAP_AIOT_ROLE | EUP_AIOT_CAP_OTA_AUTH)

struct eup_gatt_metadata {
	char project_name[EUP_GATT_PROJECT_SIZE];
	char hardware_version[EUP_GATT_HARDWARE_SIZE];
	char fw_version[EUP_GATT_FIRMWARE_SIZE];
	char build_time[EUP_GATT_BUILD_TIME_SIZE];
	char build_id[EUP_GATT_BUILD_ID_SIZE];
	char ble_name_normal[EUP_GATT_BLE_NAME_SIZE];
	char ble_name_ota[EUP_GATT_BLE_NAME_SIZE];
	char soc[EUP_GATT_SOC_SIZE];
	char device_id[EUP_GATT_DEVICE_ID_SIZE];
	uint32_t capabilities;
	uint32_t valid_mask;
	uint32_t command_success_mask;
	uint32_t last_seen_s;
	uint8_t metadata_schema;
	uint8_t protocol_version;
	uint8_t aiot_role;
	uint8_t battery;
	bool seen;
	bool battery_seen;
	bool capabilities_seen;
};

void eup_aiot_metadata_reset(struct eup_gatt_metadata *metadata);
int eup_aiot_metadata_parse(uint8_t cmd, const uint8_t *payload, size_t payload_len,
			    struct eup_gatt_metadata *metadata);

#endif /* EUP_AIOT_METADATA_H_ */
