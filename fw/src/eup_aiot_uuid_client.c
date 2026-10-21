#include "eup_aiot_uuid_client.h"

#include <errno.h>
#include <string.h>

static uint8_t sum8(const uint8_t *bytes, size_t len)
{
	uint8_t sum = 0;

	for (size_t i = 0; i < len; i++) {
		sum += bytes[i];
	}
	return sum;
}

int eup_uuid_build_request(uint8_t cmd, uint8_t seq, const uint8_t *payload,
			   size_t payload_len, uint8_t *out, size_t out_size)
{
	if (out == NULL || payload_len > UINT8_MAX || out_size < payload_len + 7U ||
	    (payload_len != 0U && payload == NULL)) {
		return -EINVAL;
	}
	out[0] = 0xe9;
	out[1] = 0x55;
	out[2] = 1;
	out[3] = cmd;
	out[4] = seq;
	out[5] = (uint8_t)payload_len;
	if (payload_len != 0U) {
		memcpy(out + 6, payload, payload_len);
	}
	out[6 + payload_len] = sum8(out, 6 + payload_len);
	return (int)(7 + payload_len);
}

int eup_uuid_parse_response(const uint8_t *data, size_t len,
			    struct eup_uuid_response *out)
{
	if (data == NULL || out == NULL || len < 10U || data[0] != 0xe9 ||
	    data[1] != 0x55 || data[2] != 1 || data[3] < 0x80 ||
	    len != 10U + data[8] || sum8(data, len - 1U) != data[len - 1U] ||
	    data[7] == 0U || data[6] >= data[7]) {
		return -EINVAL;
	}
	out->cmd = data[3] & 0x7f;
	out->seq = data[4];
	out->status = data[5];
	out->part = data[6];
	out->total = data[7];
	out->payload_len = data[8];
	out->payload = data + 9;
	return 0;
}

int eup_uuid_find_tlv(const uint8_t *data, size_t len, uint8_t tag,
		      const uint8_t **value, size_t *value_len)
{
	if (data == NULL || value == NULL || value_len == NULL) {
		return -EINVAL;
	}
	for (size_t off = 0; off + 2U <= len;) {
		uint8_t n = data[off + 1U];

		if (off + 2U + n > len) {
			return -EINVAL;
		}
		if (data[off] == tag) {
			*value = data + off + 2U;
			*value_len = n;
			return 0;
		}
		off += 2U + n;
	}
	return -ENOENT;
}
