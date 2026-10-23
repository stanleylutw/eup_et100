/* v0.8.7: device-side EUP AIoT metadata responses decoded by the FR3068's own
 * gateway-side client (eup_aiot_uuid_client + eup_aiot_metadata), i.e. the code
 * that already talks to real nRF sensors. */
#include "eup_meta_protocol.h"
#include "eup_aiot_uuid_client.h"
#include "eup_aiot_metadata.h"
#include "serial_gchk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const struct eup_fw_info info = {
    .magic = EUP_FW_INFO_MAGIC, .schema_version_le = 1, .project_name = "EUP_ET100",
    .hardware_version = "ET100_EVB_DEV1.4", .firmware_version = "0.8.7-dev",
    .build_time = "2026-10-22", .build_id = "prototype", .git_commit = "uncommitted",
    .git_branch = "v0.8.7_device_metadata", .ble_name_normal = "EUP_AIoT_HUB_FR",
    .ble_name_ota = "EUP_AIoT_HUB_FR", .release_note = "UNSIGNED prototype",
};

/* Request -> device -> every part through the client parser -> reassembled payload. */
static int roundtrip(uint8_t cmd, uint16_t mtu, uint8_t *payload, size_t *payload_len, uint8_t *status)
{
    uint8_t req[16], packet[EUP_META_RESPONSE_OVERHEAD + EUP_META_MAX_RESPONSE_PAYLOAD];
    int n = eup_uuid_build_request(cmd, 0x42, NULL, 0, req, sizeof req);
    assert(n == 7);
    struct eup_meta_identity id = { .info = &info, .mac = {0xc0, 0x32, 0x32, 0x32, 0x59, 0x03}, .mtu = mtu };
    struct eup_meta_response r;
    eup_meta_handle(req, (size_t)n, &id, &r);
    size_t chunk = eup_meta_chunk_max(mtu);
    uint8_t total = eup_meta_part_total(&r, chunk);
    *payload_len = 0;
    for (uint8_t part = 0; part < total; part++) {
        size_t len = eup_meta_encode_part(&r, chunk, part, packet, sizeof packet);
        assert(len && len <= (size_t)mtu - 3u + (mtu <= 13 ? 20u : 0u));
        struct eup_uuid_response out;
        assert(!eup_uuid_parse_response(packet, len, &out));
        assert(out.cmd == cmd && out.seq == 0x42 && out.part == part && out.total == total);
        *status = out.status;
        memcpy(payload + *payload_len, out.payload, out.payload_len);
        *payload_len += out.payload_len;
    }
    return total;
}

static void check_fw_info(uint16_t mtu)
{
    uint8_t payload[512], status;
    size_t len;
    int parts = roundtrip(0x01, mtu, payload, &len, &status);
    assert(status == 0);
    struct eup_gatt_metadata m;
    eup_aiot_metadata_reset(&m);
    assert(eup_aiot_metadata_parse(0x01, payload, len, &m) == 0);
    assert(!strcmp(m.project_name, "EUP_ET100"));
    assert(!strcmp(m.hardware_version, "ET100_EVB_DEV1.4"));
    assert(!strcmp(m.fw_version, "0.8.7-dev"));
    assert(!strcmp(m.soc, "fr3068e_c"));
    assert(!strcmp(m.ble_name_normal, "EUP_AIoT_HUB_FR"));
    assert(m.metadata_schema == 1 && m.protocol_version == 1);
    printf("  GET_FW_INFO mtu=%u -> %d part(s), %zu B\n", mtu, parts, len);
}

int main(void)
{
    check_fw_info(247);
    check_fw_info(23);                         /* default MTU: many parts */

    uint8_t payload[512], status;
    size_t len;
    struct eup_gatt_metadata m;
    roundtrip(0x05, 247, payload, &len, &status);
    eup_aiot_metadata_reset(&m);
    assert(status == 0 && eup_aiot_metadata_parse(0x05, payload, len, &m) == 0);
    assert(!strcmp(m.device_id, "C03232325903"));

    roundtrip(0x07, 247, payload, &len, &status);
    eup_aiot_metadata_reset(&m);
    assert(status == 0 && eup_aiot_metadata_parse(0x07, payload, len, &m) == 0);
    assert(m.capabilities == ((1u << 0) | (1u << 1) | (1u << 5)));

    assert(roundtrip(0x7f, 247, payload, &len, &status) == 1 && status == 0 && len == 0);
    assert(roundtrip(0x08, 247, payload, &len, &status) == 1 && status == 0x01 && len == 0);

    /* malformed requests (gateway behavior) */
    struct eup_meta_identity id = { .info = &info, .mtu = 247 };
    struct eup_meta_response r;
    uint8_t req[8];
    struct inst_snapshot ota={.ready=true,.state=BOOT_CONFIRMED,.has_record=true};
    ota.record.result=IR_ROLLED_BACK;ota.record.code=IC_TRIAL_TIMEOUT;
    ota.record.hash[0]=0xab;memset(ota.running_hash,0x5a,32);
    id.ota=&ota;
    eup_uuid_build_request(0x09,9,NULL,0,req,sizeof req);eup_meta_handle(req,7,&id,&r);
    assert(r.status==0 && r.length);
    unsigned tags=0;
    for(size_t at=0;at<r.length;) {
        unsigned tag=r.payload[at],n=r.payload[at+1];const uint8_t *value=r.payload+at+2;
        assert(at+2+n<=r.length);tags|=1u<<tag;
        if(tag==1)assert(n==1&&value[0]==BOOT_CONFIRMED);
        if(tag==2)assert(n==1&&value[0]==IR_ROLLED_BACK);
        if(tag==3)assert(n==4&&value[0]==IC_TRIAL_TIMEOUT&&!value[1]&&!value[2]&&!value[3]);
        if(tag==4)assert(n==8&&!memcmp(value,"AB000000",8));
        if(tag==5)assert(n==32&&!memcmp(value,ota.running_hash,32));
        at+=2+n;
    }
    assert(tags==0x7e);
    id.ota=NULL;eup_meta_handle(req,7,&id,&r);assert(r.status==EUP_META_INTERNAL_ERROR);
    ota.has_record=false;id.ota=&ota;eup_meta_handle(req,7,&id,&r);assert(r.status==0);
    eup_uuid_build_request(0x01, 7, NULL, 0, req, sizeof req);
    req[6] ^= 1;                                /* checksum */
    eup_meta_handle(req, 7, &id, &r);
    assert(r.status == 0x02 && r.command == 0x01 && r.sequence == 7 && r.length == 0);
    eup_uuid_build_request(0x01, 7, NULL, 0, req, sizeof req);
    req[2] = 2;                                 /* protocol version */
    eup_meta_handle(req, 7, &id, &r);
    assert(r.status == 0x05);
    req[0] = 0;                                 /* magic */
    eup_meta_handle(req, 7, &id, &r);
    assert(r.status == 0x02 && r.command == 0 && r.sequence == 0);
    eup_meta_handle(req, 3, &id, &r);           /* too short */
    assert(r.status == 0x02);
    /* full-width unterminated EUPFWV1 field must not overrun */
    struct eup_fw_info wide = info;
    memset(wide.project_name, 'P', sizeof wide.project_name);
    id.info = &wide;
    eup_uuid_build_request(0x01, 1, NULL, 0, req, sizeof req);
    eup_meta_handle(req, 7, &id, &r);
    assert(r.status == 0 && r.payload[0] == 0x01 && r.payload[1] == 24);
    puts("PASS eup meta protocol vs FR3068 client parser");
    return 0;
}
