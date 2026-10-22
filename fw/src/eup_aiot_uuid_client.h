#ifndef EUP_AIOT_UUID_CLIENT_H_
#define EUP_AIOT_UUID_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

struct eup_uuid_response {
	uint8_t cmd;
	uint8_t seq;
	uint8_t status;
	uint8_t part;
	uint8_t total;
	const uint8_t *payload;
	uint8_t payload_len;
};

int eup_uuid_build_request(uint8_t cmd, uint8_t seq, const uint8_t *payload,
			   size_t payload_len, uint8_t *out, size_t out_size);
int eup_uuid_parse_response(const uint8_t *data, size_t len,
			    struct eup_uuid_response *out);
int eup_uuid_find_tlv(const uint8_t *data, size_t len, uint8_t tag,
		      const uint8_t **value, size_t *value_len);

#endif
