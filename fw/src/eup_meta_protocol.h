#ifndef ET100_EUP_META_PROTOCOL_H
#define ET100_EUP_META_PROTOCOL_H
/* EUP AIoT Communication Service, device (OTA target) side, v0.8.7.
 * Wire-compatible with eup_gateway/src/service/eup_ota_metadata_service.c:
 * GET_FW_INFO 0x01, GET_DEVICE_ID 0x05, GET_CAPABILITIES 0x07, PING 0x7F;
 * no authentication (read-only). Spec: eup_gateway protocol_cache/02 §5–§11. */
#include <stddef.h>
#include <stdint.h>
#include "../include/eup_fw_info.h"
#include "inst_status.h"

#define EUP_META_MAX_REQUEST_PAYLOAD 244u
#define EUP_META_MAX_RESPONSE_PAYLOAD 240u
#define EUP_META_RESPONSE_OVERHEAD 10u     /* 9-byte header + checksum */
#define EUP_META_MIN_CHUNK_PAYLOAD 10u
#define EUP_META_SOC "fr3068e_c"

enum eup_meta_status { EUP_META_OK = 0x00, EUP_META_UNKNOWN_COMMAND = 0x01,
                       EUP_META_INVALID_PACKET = 0x02, EUP_META_PAYLOAD_TOO_LONG = 0x04,
                       EUP_META_UNSUPPORTED_VERSION = 0x05, EUP_META_INTERNAL_ERROR = 0x06 };

struct eup_meta_identity {
    const struct eup_fw_info *info;
    uint8_t mac[6];        /* display order, e.g. C0:32:32:32:59:03 */
    uint16_t mtu;          /* current ATT MTU (23 if not negotiated) */
    const struct inst_snapshot *ota; /* Cached only; callback must not access NOR. */
};

/* Result of one request: a response payload to be split into parts. */
struct eup_meta_response {
    uint8_t command, sequence, status;
    uint8_t length;
    uint8_t payload[EUP_META_MAX_RESPONSE_PAYLOAD];
};

/* Decode one RX write and build the response. Always produces a response
 * (as the gateway does), including for malformed packets. */
void eup_meta_handle(const uint8_t *request, size_t length,
                     const struct eup_meta_identity *id, struct eup_meta_response *out);
/* Payload bytes per notification for this MTU (gateway rule). */
size_t eup_meta_chunk_max(uint16_t mtu);
uint8_t eup_meta_part_total(const struct eup_meta_response *r, size_t chunk_max);
/* Encode part `part` into packet; returns packet length. */
size_t eup_meta_encode_part(const struct eup_meta_response *r, size_t chunk_max, uint8_t part,
                            uint8_t *packet, size_t capacity);
#endif
