#include "eup_meta_protocol.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAGIC0 0xe9
#define MAGIC1 0x55
#define VERSION 0x01
#define CAP_FW_INFO (1u << 0)
#define CAP_OTA_READY (1u << 1)
#define CAP_DEVICE_ID (1u << 5)

static uint8_t sum8(const uint8_t *p, size_t n)
{
    uint8_t s = 0;
    while (n--) s = (uint8_t)(s + *p++);
    return s;
}

struct writer { uint8_t *buf; size_t len; bool ok; };

static void put(struct writer *w, uint8_t type, const void *value, size_t n)
{
    if (!w->ok || n > UINT8_MAX || w->len + 2 + n > EUP_META_MAX_RESPONSE_PAYLOAD) {
        w->ok = false;
        return;
    }
    w->buf[w->len++] = type;
    w->buf[w->len++] = (uint8_t)n;
    memcpy(w->buf + w->len, value, n);
    w->len += n;
}
/* EUPFWV1 fields are fixed-size and may be unterminated when full. */
static void put_field(struct writer *w, uint8_t type, const char *field, size_t size)
{
    size_t n = 0;
    while (n < size && field[n]) n++;
    put(w, type, field, n);
}
static void put_u8(struct writer *w, uint8_t type, uint8_t v) { put(w, type, &v, 1); }
static void put_le16(struct writer *w, uint8_t type, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    put(w, type, b, 2);
}
static void put_le32(struct writer *w, uint8_t type, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    put(w, type, b, 4);
}

static bool fw_info(struct writer *w, const struct eup_fw_info *i)
{
    if (!i) return false;
    put_field(w, 0x01, i->project_name, sizeof i->project_name);
    put_field(w, 0x02, i->hardware_version, sizeof i->hardware_version);
    put_field(w, 0x03, i->firmware_version, sizeof i->firmware_version);
    put_field(w, 0x04, i->build_time, sizeof i->build_time);
    put_field(w, 0x05, i->build_id, sizeof i->build_id);
    put_field(w, 0x06, i->git_commit, sizeof i->git_commit);
    put_field(w, 0x07, i->git_branch, sizeof i->git_branch);
    put_field(w, 0x08, i->ble_name_normal, sizeof i->ble_name_normal);
    put_field(w, 0x09, i->ble_name_ota, sizeof i->ble_name_ota);
    put_field(w, 0x0a, i->release_note, sizeof i->release_note);
    uint16_t schema;
    memcpy(&schema, &i->schema_version_le, sizeof schema);   /* packed struct */
    put_u8(w, 0x0b, (uint8_t)schema);
    put(w, 0x0c, EUP_META_SOC, sizeof EUP_META_SOC - 1);
    put_u8(w, 0x0d, VERSION);
    return w->ok;
}

void eup_meta_handle(const uint8_t *req, size_t len,
                     const struct eup_meta_identity *id, struct eup_meta_response *out)
{
    memset(out, 0, sizeof *out);
    if (!req || len < 7 || req[0] != MAGIC0 || req[1] != MAGIC1) {
        out->status = EUP_META_INVALID_PACKET;      /* cmd 0, seq 0 (gateway) */
        return;
    }
    out->command = req[3];
    out->sequence = req[4];
    if (req[2] != VERSION) {
        out->status = EUP_META_UNSUPPORTED_VERSION;
        return;
    }
    size_t payload = req[5];
    if (payload > EUP_META_MAX_REQUEST_PAYLOAD || len != 6 + payload + 1 ||
        sum8(req, len - 1) != req[len - 1]) {
        out->status = EUP_META_INVALID_PACKET;
        return;
    }
    struct writer w = { out->payload, 0, true };
    switch (out->command) {
    case 0x01:
        if (!fw_info(&w, id->info)) out->status = EUP_META_INTERNAL_ERROR;
        break;
    case 0x05: {
        char mac[18];
        snprintf(mac, sizeof mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                 id->mac[0], id->mac[1], id->mac[2], id->mac[3], id->mac[4], id->mac[5]);
        put(&w, 0x01, mac, 17);
        if (!w.ok) out->status = EUP_META_INTERNAL_ERROR;
        break;
    }
    case 0x07:
        put_le32(&w, 0x01, CAP_FW_INFO | CAP_OTA_READY | CAP_DEVICE_ID);
        put_le16(&w, 0x02, EUP_META_MAX_REQUEST_PAYLOAD);
        put_le16(&w, 0x03, EUP_META_MAX_RESPONSE_PAYLOAD);
        put_le16(&w, 0x04, id->mtu);
        if (!w.ok) out->status = EUP_META_INTERNAL_ERROR;
        break;
    case 0x7f:
        break;
    case 0x09: {
        const struct inst_snapshot *s=id->ota;
        if (!s || !s->ready) { out->status=EUP_META_INTERNAL_ERROR;break; }
        put_u8(&w,1,(uint8_t)s->state);
        put_u8(&w,2,(uint8_t)(s->has_record ? s->record.result : IR_NONE));
        put_le32(&w,3,s->has_record ? s->record.code : 0);
        char txn[9];inst_txn(s->has_record ? s->record.hash : NULL,txn);
        put(&w,4,txn,strlen(txn));put(&w,5,s->running_hash,32);
        if (id->info) put_field(&w,6,id->info->firmware_version,sizeof id->info->firmware_version);
        if (!w.ok) out->status=EUP_META_INTERNAL_ERROR;
        break;
    }
    default:
        out->status = EUP_META_UNKNOWN_COMMAND;
        break;
    }
    out->length = out->status == EUP_META_OK ? (uint8_t)w.len : 0;
}

size_t eup_meta_chunk_max(uint16_t mtu)
{
    if (mtu <= 3) return EUP_META_MIN_CHUNK_PAYLOAD;
    size_t notify = (size_t)mtu - 3;
    if (notify <= EUP_META_RESPONSE_OVERHEAD) return EUP_META_MIN_CHUNK_PAYLOAD;
    size_t n = notify - EUP_META_RESPONSE_OVERHEAD;
    return n > EUP_META_MAX_RESPONSE_PAYLOAD ? EUP_META_MAX_RESPONSE_PAYLOAD : n;
}

uint8_t eup_meta_part_total(const struct eup_meta_response *r, size_t chunk_max)
{
    return r->length == 0 ? 1 : (uint8_t)((r->length + chunk_max - 1) / chunk_max);
}

size_t eup_meta_encode_part(const struct eup_meta_response *r, size_t chunk_max, uint8_t part,
                            uint8_t *packet, size_t capacity)
{
    uint8_t total = eup_meta_part_total(r, chunk_max);
    size_t offset = (size_t)part * chunk_max;
    size_t n = r->length > offset ? r->length - offset : 0;
    if (n > chunk_max) n = chunk_max;
    if (part >= total || capacity < 9 + n + 1) return 0;
    packet[0] = MAGIC0;
    packet[1] = MAGIC1;
    packet[2] = VERSION;
    packet[3] = (uint8_t)(r->command | 0x80);
    packet[4] = r->sequence;
    packet[5] = r->status;
    packet[6] = part;
    packet[7] = total;
    packet[8] = (uint8_t)n;
    memcpy(packet + 9, r->payload + offset, n);
    packet[9 + n] = sum8(packet, 9 + n);
    return 10 + n;
}
