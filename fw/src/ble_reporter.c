#include "ble_reporter.h"
#include <stdio.h>
#include <string.h>

void et100_reporter_init(struct et100_reporter *r, const uint16_t cids[4],
                         uint16_t gtime, uint32_t now)
{
    memset(r, 0, sizeof(*r));
    memcpy(r->cids, cids, sizeof(r->cids));
    r->gtime = gtime >= 1 && gtime <= 1800 ? gtime : 15;
    r->cycle_ms = now;
    r->maxrpt = r->gtime < 3 ? (uint8_t)r->gtime : 3;
}

void et100_reporter_controls(struct et100_reporter *r, const uint8_t macs[8][6],
                              uint8_t maxrpt, uint32_t now)
{
    if (!maxrpt || maxrpt > 15 || maxrpt > r->gtime) return;
    if (r->maxrpt == maxrpt && !memcmp(r->macs, macs, sizeof r->macs)) return;
    memcpy(r->macs, macs, sizeof r->macs);
    r->maxrpt = maxrpt;
    memset(r->cache, 0, sizeof r->cache);
    r->batch_length = r->batch_index = 0;
    r->cycle_ms = now;
}

static int allowed(const struct et100_reporter *r, const uint8_t *data)
{
    uint16_t id = data[0] | ((uint16_t)data[1] << 8);
    for (unsigned i = 0; i < 4; i++) {
        if (r->cids[i]) {
            if (r->cids[i] == id)
                return 1;
        }
    }
    return 0;
}

int et100_reporter_receive(struct et100_reporter *r, const uint8_t mac[6],
                           uint8_t type, int8_t rssi, const uint8_t *ad,
                           size_t length, uint32_t now)
{
    return et100_reporter_receive_details(r,mac,type,rssi,ad,length,now,NULL);
}

int et100_reporter_receive_details(struct et100_reporter *r, const uint8_t mac[6],
                           uint8_t type, int8_t rssi, const uint8_t *ad,
                           size_t length, uint32_t now, struct et100_adv_details *details)
{
    if(details) memset(details,0,sizeof(*details));
    const uint8_t *mfg = NULL, *svc = NULL, *name = NULL;
    const uint8_t *mfg_any = NULL, *svc_any = NULL;
    size_t mfg_any_len = 0, svc_any_len = 0;
    unsigned filters = 0, mac_match = 0;
    static const uint8_t zero[6];
    for (unsigned i = 0; i < 4; i++) filters += r->cids[i] != 0;
    for (unsigned i = 0; i < 8; i++) {
        if (!memcmp(r->macs[i], zero, 6)) continue;
        filters++;
        unsigned match = 1;
        for (unsigned j = 0; j < 6; j++)
            if (r->macs[i][j] != mac[5-j]) match = 0;
        mac_match |= match;
    }
    size_t mfg_len = 0, svc_len = 0, name_len = 0;
    uint8_t payload[31];
    if (!ad || length > 31 || rssi < -127 || rssi > 20)
        return 0;
    for (size_t offset = 0; offset < length;) {
        size_t n = ad[offset];
        if (!n)
            break;
        if (n > length - offset - 1)
            return 0;
        uint8_t kind = ad[offset + 1];
        const uint8_t *value = ad + offset + 2;
        if (n >= 3 && kind == 0xff && !mfg_any) { mfg_any = value; mfg_any_len = n - 1; }
        if (n >= 3 && kind == 0x16 && !svc_any) { svc_any = value; svc_any_len = n - 1; }
        if (n >= 3 && (kind == 0xff || kind == 0x16) && allowed(r, value)) {
            if (kind == 0xff && !mfg) { mfg = value; mfg_len = n - 1; }
            if (kind == 0x16 && !svc) { svc = value; svc_len = n - 1; }
        }
        if (!name && (kind == 0x08 || kind == 0x09) && n > 1) {
            name = ad + offset;
            name_len = n - 1 > 12 ? 12 : n - 1;
        }
        offset += n + 1;
    }
    /* Gateway semantics: CID OR MAC, with CID-matched payload preferred. */
    if (!mfg && !svc && (!filters || mac_match)) {
        mfg = mfg_any; mfg_len = mfg_any_len;
        svc = svc_any; svc_len = svc_any_len;
    }
    const uint8_t *value = mfg ? mfg : svc;
    size_t n = mfg ? mfg_len : svc_len;
    if (!value)
        return 0;
    memcpy(payload, value, n);
    uint8_t version = 0, token = 0;
    /* Same v1/v2 classification and packet-info offsets as Gateway parser. */
    if (mfg && n == 19 && (value[8] >> 4) == 0) { version = 1; token = value[11]; }
    if (mfg && n == 16 && (value[2] >> 4) == 1) { version = 2; token = value[8]; }
    uint8_t detail_version=version;
    if(!mfg && n==19 && (value[8]>>4)==0) detail_version=1;
    if(!mfg && n==16 && (value[2]>>4)==1) detail_version=2;
    if(details && detail_version) {
        details->version=detail_version;details->token=value[detail_version==1?11:8];
        uint8_t packed=value[detail_version==1?9:6];
        details->mode=packed&7;details->state=(packed>>3)&7;
        if(detail_version==1) memcpy(details->identity,value+2,6);
        else {
            memcpy(details->identity,mac,6);memcpy(details->firmware,value+3,3);
        }
        for(unsigned i=0;i<6;i++) if(details->identity[i]) details->identity_ready=1;
    }
    if (name && n < 31) {
        uint8_t structure[14];
        structure[0] = (uint8_t)(name_len + 1);
        structure[1] = name[1];
        memcpy(structure + 2, name + 2, name_len);
        size_t append = name_len + 2;
        if (append > 31 - n) append = 31 - n;
        memcpy(payload + n, structure, append);
        n += append;
    }

    unsigned slot = ET100_REPORT_CACHE;
    for (unsigned i = 0; i < ET100_REPORT_CACHE; i++)
        if (r->cache[i].used && r->cache[i].address_type == type &&
            memcmp(r->cache[i].mac, mac, 6) == 0) { slot = i; break; }
    if (slot == ET100_REPORT_CACHE) {
        slot = 0;
        for (unsigned i = 0; i < ET100_REPORT_CACHE; i++) {
            if (!r->cache[i].used) { slot = i; break; }
            if ((uint32_t)(now - r->cache[i].seen_ms) >
                (uint32_t)(now - r->cache[slot].seen_ms)) slot = i;
        }
        memset(&r->cache[slot], 0, sizeof(r->cache[slot]));
    }
    struct et100_report_entry *entry = &r->cache[slot];
    entry->rssi = rssi;
    entry->seen_ms = now;
    if (version && entry->reported && entry->reported_version == version &&
        entry->reported_token == token && (uint32_t)(now - entry->reported_ms) < 1500)
        return 1;
    memcpy(entry->mac, mac, 6);
    entry->address_type = type;
    memcpy(entry->payload, payload, n);
    entry->length = (uint8_t)n;
    entry->version = version;
    entry->token = token;
    entry->used = entry->updated = 1;
    return 1;
}

size_t et100_reporter_poll(struct et100_reporter *r, uint32_t now,
                          char *output, size_t capacity)
{
    if (capacity < 104)
        return 0;
    if ((uint32_t)(now - r->cycle_ms) >= (uint32_t)r->gtime * 1000) {
        r->cycle_ms = now;
        r->batch_length = r->batch_index = 0;
        unsigned limit = r->maxrpt;
        for (unsigned count = 0; count < limit; count++) {
            int best = -1;
            for (unsigned i = 0; i < ET100_REPORT_CACHE; i++)
                if (r->cache[i].updated && (best < 0 ||
                    r->cache[i].rssi > r->cache[best].rssi)) best = (int)i;
            if (best < 0) break;
            struct et100_report_entry *entry = &r->cache[best];
            r->batch[r->batch_length++] = *entry;
            entry->updated = 0;
            entry->reported = entry->version != 0;
            entry->reported_version = entry->version;
            entry->reported_token = entry->token;
            entry->reported_ms = now;
        }
        for (unsigned i = 0; i < ET100_REPORT_CACHE; i++) r->cache[i].updated = 0;
    }
    if (r->batch_index == r->batch_length ||
        (r->sent_valid && (uint32_t)(now - r->sent_ms) < 1000)) return 0;
    const struct et100_report_entry *entry = &r->batch[r->batch_index++];
    char hex[63];
    for (unsigned i = 0; i < entry->length; i++)
        snprintf(hex + 2 * i, 3, "%02X", entry->payload[i]);
    hex[2 * entry->length] = '\0';
    int magnitude = entry->rssi < 0 ? -entry->rssi : entry->rssi;
    int n = snprintf(output, capacity,
        "AT$PD=G1;1;%02X%02X%02X%02X%02X%02X;%s;%c%03d",
        entry->mac[5], entry->mac[4], entry->mac[3], entry->mac[2],
        entry->mac[1], entry->mac[0], hex, entry->rssi < 0 ? '0' : '1', magnitude);
    if (n < 6 || (size_t)n + 6 > capacity) return 0;
    uint8_t checksum = 0;
    for (int i = 6; i < n; i++) checksum ^= (uint8_t)output[i];
    snprintf(output + n, capacity - (size_t)n, "*%02X\r\n", checksum);
    r->sent_valid = 1;
    r->sent_ms = now;
    return (size_t)n + 5;
}

#ifndef ET100_SERIAL_HOST_TEST
#include "FreeRTOS.h"
#include "task.h"
#include "gap_api.h"
#include "app_gatt_server.h"
static struct et100_reporter reporter;
static int configured;
volatile unsigned et100_scan_status = 0xff;
volatile unsigned et100_scan_reports, et100_scan_accepted;
static uint32_t scan_attempt_ms;
static unsigned scan_attempted;

void et100_scan_configure(const uint16_t cids[4], uint16_t gtime,
                          const uint8_t macs[8][6], uint8_t maxrpt, int clear)
{
    uint32_t now = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    taskENTER_CRITICAL();
    if (!configured || clear) {
        et100_app_server_clear();
        et100_reporter_init(&reporter, cids, gtime, now);
        configured = 1;
    } else if (reporter.gtime != gtime) {
        reporter.gtime = gtime;
        reporter.cycle_ms = now;
        reporter.batch_length = reporter.batch_index = 0;
    }
    /* Publish CID, MAC and batch limit atomically to the BLE callback. */
    et100_reporter_controls(&reporter, macs, maxrpt, now);
    taskEXIT_CRITICAL();
}

void et100_scan_poll(void (*send)(const char *, size_t, void *), void *context)
{
    char output[128];
    uint32_t now = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    taskENTER_CRITICAL();
    size_t length = configured ? et100_reporter_poll(&reporter, now, output, sizeof(output)) : 0;
    taskEXIT_CRITICAL();
    if (length) send(output, length, context);
}

void et100_scan_start(void)
{
    static gap_scan_param_t params = {
        /* Use the SDK central example's discovery mode to avoid host-side
         * filtering of general-discoverable packets in observer modes. */
        .scan_type = GAP_SCAN_TYPE_GEN_DISC,
        .own_addr_type = GAP_ADDR_TYPE_STATIC,
        .phy_mode = GAPM_PHY_TYPE_LE_1M,
        .dup_filt_pol = 0, .scan_intv = 40, .scan_window = 30, .duration = 0,
    };
    uint32_t now = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    taskENTER_CRITICAL();
    unsigned status = et100_scan_status;
    if (status == 0 || (scan_attempted && (uint32_t)(now - scan_attempt_ms) < 1000) ||
        (status == 0xfe && (uint32_t)(now - scan_attempt_ms) < 3000)) {
        taskEXIT_CRITICAL(); return;
    }
    scan_attempt_ms = now; scan_attempted = 1; et100_scan_status = 0xfe;
    taskEXIT_CRITICAL();
    ble_api_err_t result = gap_scan_start(&params);
    taskENTER_CRITICAL();
    if (result != BLE_API_ERR_NO_ERR && et100_scan_status == 0xfe) et100_scan_status = 0xfd;
    taskEXIT_CRITICAL();
}

void et100_scan_event(struct gap_event *event)
{
    if (event->type == GAP_EVT_SCAN_START) et100_scan_status = event->param.scan_start_status ?
        0x200 | event->param.scan_start_status : 0;
    if (event->type == GAP_EVT_SCAN_END) et100_scan_status = 0x100 | event->param.scan_end_status;
    if (event->type != GAP_EVT_ADV_REPORT || !event->param.adv_rpt) return;
    struct gap_evt_adv_report *report = event->param.adv_rpt;
    /* Only complete legacy advertising; never merge scan responses into payload. */
    if (report->evt_type & (GAP_SCAN_EVT_SCAN_RSP | GAP_SCAN_EVT_EXT_ADV | GAP_SCAN_EVT_PER_ADV)) return;
    uint32_t now = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    taskENTER_CRITICAL();
    et100_scan_reports++;
    if (configured) {
        struct et100_adv_details details;
        int accepted=et100_reporter_receive_details(&reporter, report->addr.addr.addr,
            report->addr.addr_type, report->rssi, report->data, report->length, now, &details);
        et100_scan_accepted += accepted;
        if(accepted) et100_app_server_receive(report->addr.addr.addr,report->addr.addr_type,
            report->rssi,report->evt_type==GAP_SCAN_EVT_NONCONN_NONSCAN?3:report->evt_type,
            report->data,report->length,&details,now);
    }
    taskEXIT_CRITICAL();
}
#endif
