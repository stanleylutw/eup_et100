/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "eup_aiot_metadata.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

struct tlv_view {
	uint8_t tag;
	const uint8_t *value;
	size_t len;
};

static int next_tlv(const uint8_t *payload, size_t payload_len, size_t *offset,
		    struct tlv_view *tlv)
{
	if (*offset == payload_len) {
		return 0;
	}
	if (payload_len - *offset < 2U) {
		return -EBADMSG;
	}
	tlv->tag = payload[*offset];
	tlv->len = payload[*offset + 1U];
	*offset += 2U;
	if (tlv->len > payload_len - *offset) {
		return -EBADMSG;
	}
	tlv->value = payload + *offset;
	*offset += tlv->len;
	return 1;
}

static int copy_text(char *out, size_t out_size, const struct tlv_view *tlv)
{
	if (tlv->len == 0U || tlv->len >= out_size) {
		return -EMSGSIZE;
	}
	for (size_t i = 0U; i < tlv->len; i++) {
		if (tlv->value[i] == '\0' || tlv->value[i] == ';' || tlv->value[i] == '\r' ||
		    tlv->value[i] == '\n') {
			return -EINVAL;
		}
	}
	memcpy(out, tlv->value, tlv->len);
	out[tlv->len] = '\0';
	return 0;
}

static int normalize_device_id(char out[EUP_GATT_DEVICE_ID_SIZE], const struct tlv_view *tlv)
{
	size_t written = 0U;

	for (size_t i = 0U; i < tlv->len; i++) {
		unsigned char ch = tlv->value[i];

		if (ch == ':' || ch == '-') {
			continue;
		}
		if (!isxdigit(ch) || written >= EUP_GATT_DEVICE_ID_SIZE - 1U) {
			return -EINVAL;
		}
		out[written++] = (char)toupper(ch);
	}
	if (written != EUP_GATT_DEVICE_ID_SIZE - 1U) {
		return -EINVAL;
	}
	out[written] = '\0';
	return 0;
}

static int parse_fw_info(const uint8_t *payload, size_t payload_len,
			 struct eup_gatt_metadata *metadata)
{
	size_t offset = 0U;
	uint32_t seen = 0U;
	struct tlv_view tlv;
	int next;

	while ((next = next_tlv(payload, payload_len, &offset, &tlv)) > 0) {
		uint32_t bit = tlv.tag >= 1U && tlv.tag <= 13U ? (1UL << (tlv.tag - 1U)) : 0U;
		int err = 0;

		if (bit != 0U && (seen & bit) != 0U) {
			return -EEXIST;
		}
		seen |= bit;
		switch (tlv.tag) {
		case 0x01:
			err = copy_text(metadata->project_name, sizeof(metadata->project_name),
					&tlv);
			metadata->valid_mask |= EUP_GATT_META_PROJECT;
			break;
		case 0x02:
			err = copy_text(metadata->hardware_version,
					sizeof(metadata->hardware_version), &tlv);
			metadata->valid_mask |= EUP_GATT_META_HARDWARE;
			break;
		case 0x03:
			err = copy_text(metadata->fw_version, sizeof(metadata->fw_version), &tlv);
			metadata->valid_mask |= EUP_GATT_META_FIRMWARE;
			break;
		case 0x04:
			err = copy_text(metadata->build_time, sizeof(metadata->build_time), &tlv);
			metadata->valid_mask |= EUP_GATT_META_BUILD_TIME;
			break;
		case 0x05:
			err = copy_text(metadata->build_id, sizeof(metadata->build_id), &tlv);
			metadata->valid_mask |= EUP_GATT_META_BUILD_ID;
			break;
		case 0x08:
			err = copy_text(metadata->ble_name_normal,
					sizeof(metadata->ble_name_normal), &tlv);
			metadata->valid_mask |= EUP_GATT_META_BLE_NORMAL;
			break;
		case 0x09:
			err = copy_text(metadata->ble_name_ota, sizeof(metadata->ble_name_ota),
					&tlv);
			metadata->valid_mask |= EUP_GATT_META_BLE_OTA;
			break;
		case 0x0b:
			if (tlv.len != 1U) {
				return -EBADMSG;
			}
			metadata->metadata_schema = tlv.value[0];
			metadata->valid_mask |= EUP_GATT_META_SCHEMA;
			break;
		case 0x0c:
			err = copy_text(metadata->soc, sizeof(metadata->soc), &tlv);
			metadata->valid_mask |= EUP_GATT_META_SOC;
			break;
		case 0x0d:
			if (tlv.len != 1U) {
				return -EBADMSG;
			}
			metadata->protocol_version = tlv.value[0];
			metadata->valid_mask |= EUP_GATT_META_PROTOCOL;
			break;
		default:
			break;
		}
		if (err != 0) {
			return err;
		}
	}
	if (next < 0) {
		return next;
	}
	const uint32_t required = EUP_GATT_META_PROJECT | EUP_GATT_META_HARDWARE |
				  EUP_GATT_META_FIRMWARE | EUP_GATT_META_BUILD_ID |
				  EUP_GATT_META_SOC | EUP_GATT_META_SCHEMA | EUP_GATT_META_PROTOCOL;
	return (metadata->valid_mask & required) == required ? 0 : -ENODATA;
}

void eup_aiot_metadata_reset(struct eup_gatt_metadata *metadata)
{
	if (metadata != NULL) {
		memset(metadata, 0, sizeof(*metadata));
	}
}

static int parse_command(uint8_t cmd, const uint8_t *payload, size_t payload_len,
			    struct eup_gatt_metadata *metadata)
{
	struct tlv_view tlv;
	size_t offset = 0U;
	int next;
	int err = 0;

	if (metadata == NULL || (payload_len != 0U && payload == NULL)) {
		return -EINVAL;
	}
	if (cmd == 0x01U) {
		err = parse_fw_info(payload, payload_len, metadata);
		goto done;
	}
	while ((next = next_tlv(payload, payload_len, &offset, &tlv)) > 0) {
		if (tlv.tag != 0x01U) {
			continue;
		}
		if (cmd == 0x04U) {
			if (tlv.len != 1U || metadata->battery_seen) {
				return -EBADMSG;
			}
			metadata->battery = tlv.value[0];
			metadata->battery_seen = true;
		} else if (cmd == 0x05U) {
			if ((metadata->valid_mask & EUP_GATT_META_DEVICE_ID) != 0U) {
				return -EEXIST;
			}
			err = normalize_device_id(metadata->device_id, &tlv);
			if (err == 0) {
				metadata->valid_mask |= EUP_GATT_META_DEVICE_ID;
			}
		} else if (cmd == 0x07U) {
			if (tlv.len != 4U || metadata->capabilities_seen) {
				return -EBADMSG;
			}
			metadata->capabilities =
				(uint32_t)tlv.value[0] | ((uint32_t)tlv.value[1] << 8) |
				((uint32_t)tlv.value[2] << 16) | ((uint32_t)tlv.value[3] << 24);
			metadata->capabilities_seen = true;
			metadata->valid_mask |= EUP_GATT_META_CAPABILITIES;
		} else if (cmd == 0x08U) {
			if (tlv.len != 1U ||
			    (metadata->valid_mask & EUP_GATT_META_AIOT_ROLE) != 0U) {
				return -EBADMSG;
			}
			metadata->aiot_role = tlv.value[0];
			metadata->valid_mask |= EUP_GATT_META_AIOT_ROLE;
		}
		if (err != 0) {
			return err;
		}
	}
	if (next < 0) {
		return next;
	}
	if (cmd == 0x05U && (metadata->valid_mask & EUP_GATT_META_DEVICE_ID) == 0U) {
		err = -ENODATA;
	}
	if (cmd == 0x07U && (metadata->valid_mask & EUP_GATT_META_CAPABILITIES) == 0U) {
		err = -ENODATA;
	}
	if (cmd == 0x08U && (metadata->valid_mask & EUP_GATT_META_AIOT_ROLE) == 0U) {
		err = -ENODATA;
	}
done:
	if (err == 0 && cmd >= 1U && cmd <= 8U) {
		metadata->command_success_mask |= EUP_GATT_CMD_SUCCESS(cmd);
		metadata->seen = true;
	}
	return err;
}

int eup_aiot_metadata_parse(uint8_t cmd, const uint8_t *payload, size_t payload_len,
			    struct eup_gatt_metadata *metadata)
{
	struct eup_gatt_metadata candidate;
	uint32_t fields = 0U;
	int err;

	if (metadata == NULL || (payload_len != 0U && payload == NULL)) {
		return -EINVAL;
	}
	if (cmd == 1U) {
		fields = EUP_GATT_META_PROJECT | EUP_GATT_META_HARDWARE | EUP_GATT_META_FIRMWARE |
			 EUP_GATT_META_BUILD_TIME | EUP_GATT_META_BUILD_ID | EUP_GATT_META_BLE_NORMAL |
			 EUP_GATT_META_BLE_OTA | EUP_GATT_META_SOC | EUP_GATT_META_SCHEMA |
			 EUP_GATT_META_PROTOCOL;
	} else if (cmd == 5U) {
		fields = EUP_GATT_META_DEVICE_ID;
	} else if (cmd == 7U) {
		fields = EUP_GATT_META_CAPABILITIES;
	} else if (cmd == 8U) {
		fields = EUP_GATT_META_AIOT_ROLE;
	}
	/* A failed refresh invalidates that command; partial TLVs never become evidence. */
	metadata->valid_mask &= ~fields;
	if (cmd >= 1U && cmd <= 8U) {
		metadata->command_success_mask &= ~EUP_GATT_CMD_SUCCESS(cmd);
	}
	if (cmd == 4U) {
		metadata->battery_seen = false;
	}
	if (cmd == 7U) {
		metadata->capabilities_seen = false;
	}
	candidate = *metadata;
	err = parse_command(cmd, payload, payload_len, &candidate);
	if (err == 0) {
		*metadata = candidate;
	}
	return err;
}
