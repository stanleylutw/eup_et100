#include "gatt_client.h"
#include "ble_reporter.h"
#include "eup_aiot_uuid_client.h"
#include "eup_aiot_metadata.h"
#include "eup_aiot_auth.h"
#include "fr30xx.h"
#include "gap_api.h"
#include "gatt_api.h"
#include "fdb_app.h"
#include "et100_fdb_keys.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>

#define AUTO_KEY ET100_FDB_AUTO_LIST_KEY
#define RAW_MAX 320
#define OUT_MAX 256
enum session_state { IDLE, WAIT_ADV, CONNECTING, SETUP, AUTH, READY, DRAIN };
static volatile enum session_state state;
static volatile unsigned cancel_requested, connected, initiating, matched, scan_ended;
static volatile unsigned scan_stop_pending, connection_end_seen;
static uint8_t connection_end_status;
static volatile unsigned op_done, response_done, mtu_done, output_fault;
static uint8_t client_id = 0xff, conidx, expected_op, op_status, seq;
static uint8_t target[6], cache_mac[6], auto_macs[8][6];
static struct gap_ble_addr peer;
static uint16_t service_start, service_end, rx_handle, tx_handle, ccc_handle, tx_end, mtu;
static uint8_t expected_cmd, expected_seq, part, total, response_status, response[RAW_MAX];
static size_t response_length;
static uint32_t connected_ms, cache_ms;
static struct eup_gatt_metadata metadata;
static QueueHandle_t output_queue;
struct queued_output { char line[OUT_MAX]; unsigned generation, ready_only; };
static unsigned session_generation;
static TaskHandle_t uart_task;
static void (*uart_send)(const char *, size_t, void *);
static void *uart_context;
static unsigned scan_resume_pending;
static volatile unsigned radio_ready;
static int db_ready, manual, request_pending;
static uint32_t cooldown_ms;
static unsigned cooldown_valid;
static struct {
    uint8_t command, payload[32];
    size_t length;
    char name[16], fields[112], token[5];
} request;

static const struct gatt_uuid service_uuid = {16,
    {1,0,0,0x50,0x55,0x45,0x43,0x49,0x47,0x4f,0x4c,0x4f,0,0,0xe0,0xe9}};

static uint32_t now_ms(void) { return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS; }
static void mac_text(const uint8_t mac[6], char out[13])
{
    snprintf(out, 13, "%02X%02X%02X%02X%02X%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}
static int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int unhex(const char *text, uint8_t *bytes, size_t capacity)
{
    if (!text) return -1;
    size_t n = strlen(text);
    if ((n & 1) || n/2 > capacity) return -1;
    for (size_t i = 0; i < n/2; i++) {
        int a = nibble(text[i*2]), b = nibble(text[i*2+1]);
        if (a < 0 || b < 0) return -1;
        bytes[i] = (uint8_t)((a << 4) | b);
    }
    return (int)(n/2);
}
static void queue_output(const char *text, unsigned ready_only)
{
    struct queued_output item = {0};
    int n = snprintf(item.line, sizeof(item.line), "%s\r\n", text);
    if (n <= 0 || n >= (int)sizeof(item.line)) { output_fault = 1; return; }
    item.generation = session_generation;
    item.ready_only = ready_only;
    if (uart_task && xTaskGetCurrentTaskHandle() == uart_task) {
        /* RX dispatch never transmits or waits for queue space. */
        if (xQueueSend(output_queue, &item, 0) != pdPASS) {
            output_fault = 1; cancel_requested = 1;
        }
        return;
    }
    uint32_t start = now_ms();
    do {
        taskENTER_CRITICAL();
        int sent = uxQueueMessagesWaiting(output_queue) < 12 && xQueueSend(output_queue, &item, 0) == pdPASS;
        taskEXIT_CRITICAL();
        if (sent) return;
        vTaskDelay(pdMS_TO_TICKS(5));
    } while ((uint32_t)(now_ms() - start) < 500);
    output_fault = 1;
}
static void queue_line(const char *text) { queue_output(text, 0); }
static void reply(const char *command, int valid, const char *fields, const char *token)
{
    char line[OUT_MAX];
    int n = snprintf(line, sizeof(line), "AT$PD=BLEGW$%s;%u;%s;%s", command, valid, fields, token);
    if (n > 0 && n < (int)sizeof(line)) queue_line(line);
}
static void report(const char *body, unsigned ready_only)
{
    char line[OUT_MAX];
    uint8_t checksum = 0;
    for (const char *p = body; *p; p++) checksum ^= (uint8_t)*p;
    int n = snprintf(line, sizeof(line), "AT$PD=%s*%02X", body, checksum);
    if (n > 0 && n < (int)sizeof(line)) queue_output(line, ready_only);
}
static void event(const char *name, const char *data)
{
    char body[120], mac[13];
    mac_text(target, mac);
    snprintf(body, sizeof(body), "G1;8;%s;%s;%s", name, mac, data);
    report(body, !strcmp(name, "AP"));
}
static int auto_enabled(const uint8_t mac[6])
{
    int enabled = 0;
    taskENTER_CRITICAL();
    for (unsigned i = 0; i < 8; i++)
        if (memcmp(auto_macs[i], mac, 6) == 0) enabled = 1;
    taskEXIT_CRITICAL();
    return enabled;
}

static int save_auto(const uint8_t mac[6], int enable)
{
    uint8_t next[8][6], blob[100] = {'E','A',1,0}, verify[100];
    int slot = -1, free_slot = -1;
    if (!db_ready) return 0;
    taskENTER_CRITICAL();
    memcpy(next, auto_macs, sizeof(next));
    taskEXIT_CRITICAL();
    for (unsigned i = 0; i < 8; i++) {
        static const uint8_t zero[6];
        if (memcmp(next[i], mac, 6) == 0) slot = (int)i;
        if (memcmp(next[i], zero, 6) == 0) free_slot = (int)i;
    }
    if (enable && slot < 0) {
        if (free_slot < 0) return 0;
        memcpy(next[free_slot], mac, 6);
    } else if (!enable && slot >= 0) memset(next[slot], 0, 6);
    memcpy(blob + 4, next, 48);
    for (unsigned i = 0; i < 48; i++) blob[52+i] = (uint8_t)~blob[4+i];
    int same = flashdb_get_length(AUTO_KEY) == sizeof(blob) &&
        flashdb_get(AUTO_KEY, verify, sizeof(verify)) == sizeof(verify) &&
        memcmp(blob, verify, sizeof(blob)) == 0;
    if (!same && (flashdb_set(AUTO_KEY, blob, sizeof(blob)) != FDB_NO_ERR ||
        flashdb_get(AUTO_KEY, verify, sizeof(verify)) != sizeof(verify) ||
        memcmp(blob, verify, sizeof(blob)) != 0)) return 0;
    taskENTER_CRITICAL();
    memcpy(auto_macs, next, sizeof(next));
    taskEXIT_CRITICAL();
    return 1;
}

static int uuid_is(const struct gatt_uuid *uuid, unsigned suffix)
{
    uint8_t bytes[16];
    memcpy(bytes, service_uuid.p_uuid, 16);
    bytes[12] = (uint8_t)suffix;
    return uuid->size == 16 && memcmp(uuid->p_uuid, bytes, 16) == 0;
}
static uint16_t client_callback(struct gatt_msg *message)
{
    if (!connected || message->conn_idx != conidx) return 0;
    taskENTER_CRITICAL();
    switch (message->msg_evt) {
    case GATTC_MSG_CMP_EVT:
        if (message->param.gatt_op_cmp.operation == expected_op) {
            op_status = message->param.gatt_op_cmp.status;
            op_done = 1;
        }
        break;
    case GATTC_MSG_SVC_REPORT:
        if (expected_op == GATT_OP_PEER_SVC_DISC_END &&
            message->param.gatt_data.msg_len >= sizeof(gatt_svc_report_t)) {
            gatt_svc_report_t value;
            memcpy(&value, message->param.gatt_data.p_msg_data, sizeof(value));
            if (uuid_is(&value.uuid, 0)) { service_start = value.start_hdl; service_end = value.end_hdl; }
        }
        break;
    case GATTC_MSG_CHAR_REPORT:
        if (expected_op == GATT_OP_PEER_CHAR_DISC_END &&
            message->param.gatt_data.msg_len >= sizeof(gatt_char_report_t)) {
            gatt_char_report_t value;
            memcpy(&value, message->param.gatt_data.p_msg_data, sizeof(value));
            if (uuid_is(&value.uuid, 1) && (value.prop & GATT_PROP_WRITE_CMD)) rx_handle = value.val_hdl;
            if (uuid_is(&value.uuid, 2) && (value.prop & GATT_PROP_NOTI)) tx_handle = value.val_hdl;
            if (tx_handle && value.char_hdl > tx_handle && value.char_hdl <= tx_end)
                tx_end = value.char_hdl - 1;
        }
        break;
    case GATTC_MSG_CHAR_DESC_REPORT:
        if (expected_op == GATT_OP_PEER_DESC_DISC_END &&
            message->param.gatt_data.msg_len >= sizeof(gatt_desc_report_t)) {
            gatt_desc_report_t value;
            memcpy(&value, message->param.gatt_data.p_msg_data, sizeof(value));
            if (value.uuid.size == 2 && value.uuid.p_uuid[0] == 2 && value.uuid.p_uuid[1] == 0x29)
                ccc_handle = value.desc_hdl;
        }
        break;
    case GATTC_MSG_NTF_REQ: {
        struct eup_uuid_response value;
        if (message->handle == tx_handle && !response_done &&
            eup_uuid_parse_response(message->param.gatt_data.p_msg_data,
                message->param.gatt_data.msg_len, &value) == 0 &&
            value.cmd == expected_cmd && value.seq == expected_seq && value.part == part &&
            (part == 0 || (value.total == total && value.status == response_status)) &&
            response_length + value.payload_len <= sizeof(response)) {
            total = value.total;
            response_status = value.status;
            memcpy(response + response_length, value.payload, value.payload_len);
            response_length += value.payload_len;
            if (++part == total) response_done = 1;
        }
        break;
    }
    default: break;
    }
    taskEXIT_CRITICAL();
    return 0;
}

static int wait_flag(volatile unsigned *flag, uint32_t timeout)
{
    uint32_t start = now_ms();
    while (!*flag && connected && !cancel_requested && !output_fault &&
           (uint32_t)(now_ms() - start) < timeout) vTaskDelay(pdMS_TO_TICKS(5));
    return *flag && connected && !cancel_requested && !output_fault;
}
static void begin_op(uint8_t operation)
{
    taskENTER_CRITICAL();
    expected_op = operation; op_done = 0; op_status = 0xff;
    taskEXIT_CRITICAL();
}
static int finish_op(void)
{
    return wait_flag(&op_done, 2000) && (op_status == 0 || op_status == 0x0a);
}
static int command(uint8_t cmd, const uint8_t *payload, size_t length, uint32_t timeout)
{
    static uint8_t packet[48];
    if (!connected || cancel_requested || mtu < 35) return 0;
    int n = eup_uuid_build_request(cmd, ++seq, payload, length, packet, sizeof(packet));
    if (n < 0 || n > mtu - 3) return 0;
    taskENTER_CRITICAL();
    expected_cmd = cmd; expected_seq = seq; response_done = 0;
    response_length = part = total = response_status = 0;
    taskEXIT_CRITICAL();
    struct gatt_client_write_param_handle write = {
        .conidx = conidx, .client_id = client_id, .handle = rx_handle,
        .p_data = packet, .data_len = (uint16_t)n,
    };
    return gatt_cilent_write_cmd_with_handle(&write) == BLE_API_ERR_NO_ERR &&
           wait_flag(&response_done, timeout);
}
static int setup(void)
{
    mtu_done = 0; mtu = 23;
    if (gatt_mtu_exchange_req(client_id, conidx, 247) != BLE_API_ERR_NO_ERR ||
        !wait_flag(&mtu_done, 1500) || mtu < 35) return 0;
    service_start = service_end = rx_handle = tx_handle = ccc_handle = 0;
    begin_op(GATT_OP_PEER_SVC_DISC_END);
    struct gatt_uuid uuid = service_uuid;
    if (gatt_discovery_peer_svc(client_id, conidx, &uuid) != BLE_API_ERR_NO_ERR ||
        !finish_op() || !service_start || service_end <= service_start) return 0;
    tx_end = service_end;
    begin_op(GATT_OP_PEER_CHAR_DISC_END);
    if (gatt_discovery_peer_char(client_id, conidx, service_start, service_end) != BLE_API_ERR_NO_ERR ||
        !finish_op() || !rx_handle || !tx_handle || tx_end <= tx_handle) return 0;
    begin_op(GATT_OP_PEER_DESC_DISC_END);
    if (gatt_discovery_peer_desc(client_id, conidx, tx_handle + 1, tx_end) != BLE_API_ERR_NO_ERR ||
        !finish_op() || !ccc_handle) return 0;
    static uint8_t enable[2] = {1,0};
    begin_op(GATT_OP_WRITE_REQ);
    struct gatt_client_write_param_handle write = {
        .conidx = conidx, .client_id = client_id, .handle = ccc_handle,
        .p_data = enable, .data_len = 2,
    };
    return gatt_cilent_write_req_with_handle(&write) == BLE_API_ERR_NO_ERR && finish_op() && op_status == 0;
}
static uint32_t auth_remaining(void)
{
    uint32_t elapsed = now_ms() - connected_ms;
    return elapsed < 3000 ? 3000 - elapsed : 0;
}
static int authenticate(void)
{
    uint8_t token[16], payload[18];
    const uint8_t *nonce;
    size_t length;
    state = AUTH; event("AS", "");
    uint32_t remaining = auth_remaining();
    if (!remaining || !command(0x10, NULL, 0, remaining < 900 ? remaining : 900) || response_status ||
        eup_uuid_find_tlv(response, response_length, 1, &nonce, &length) || length != 16 ||
        eup_aiot_auth_compute_token(nonce, token)) return 0;
    payload[0] = 1; payload[1] = 16; memcpy(payload + 2, token, 16);
    remaining = auth_remaining();
    int ok = remaining && command(0x11, payload, sizeof(payload), remaining < 900 ? remaining : 900) &&
             response_status == 0 && auth_remaining();
    memset(token, 0, sizeof(token)); memset(payload, 0, sizeof(payload));
    return ok;
}
static void chunks(uint8_t cmd, const char *name, const char *token)
{
    char mac[13], body[220], hex[129], fields[220];
    mac_text(target, mac);
    unsigned count = response_length ? (unsigned)((response_length + 63)/64) : 1;
    for (unsigned p = 0; p < count; p++) {
        size_t offset = p*64, n = response_length - offset;
        if (n > 64) n = 64;
        for (size_t i = 0; i < n; i++) snprintf(hex + i*2, 3, "%02X", response[offset+i]);
        hex[n*2] = 0;
        char suffix[16] = "";
        if (count > 1) snprintf(suffix, sizeof(suffix), ";p%u/%u", p+1,count);
        if (!name) {
            snprintf(body, sizeof(body), "G1;8;MR;%s;%02X;%02X;%s%s", mac,cmd,response_status,hex,suffix);
            report(body, 1);
        } else {
            snprintf(fields, sizeof(fields), "%s;%02X;%02X;%s%s", mac,cmd,response_status,hex,suffix);
            reply(name, 1, fields, token);
        }
    }
}

static void complete_request(int ok)
{
    if (!strcmp(request.name, "SSENSORCFG"))
        reply(request.name, ok && response_status == 0, request.fields, request.token);
    else if (ok) chunks(request.command, request.name, request.token);
    else {
        char mac[13], fields[40]; mac_text(target, mac);
        snprintf(fields, sizeof(fields), "%s;%02X;00;", mac, request.command);
        reply(request.name, 0, fields, request.token);
    }
    taskENTER_CRITICAL(); request_pending = 0; taskEXIT_CRITICAL();
}

static void update_metadata_cache(uint8_t cmd)
{
    if (cmd < 1 || cmd > 8 || response_status != 0) return;
    taskENTER_CRITICAL();
    if (eup_aiot_metadata_parse(cmd, response, response_length, &metadata) == 0)
        cache_ms = now_ms();
    taskEXIT_CRITICAL();
}

static int release_owner(void)
{
    char mac[13], body[64];
    struct queued_output item = {0};
    mac_text(target, mac);
    snprintf(body, sizeof(body), "G1;8;DC;%s;", mac);
    uint8_t checksum = 0;
    for (const char *p = body; *p; p++) checksum ^= (uint8_t)*p;
    snprintf(item.line, sizeof(item.line), "AT$PD=%s*%02X\r\n", body, checksum);
    item.generation = session_generation;
    taskENTER_CRITICAL();
    int released = !initiating && !connected && !scan_stop_pending &&
                   xQueueSend(output_queue, &item, 0) == pdPASS;
    if (released) {
        state = IDLE; matched = cancel_requested = output_fault = 0;
        cooldown_ms = now_ms(); cooldown_valid = 1;
    }
    taskEXIT_CRITICAL();
    return released;
}

static void worker(void *argument)
{
    (void)argument;
    for (;;) {
        if (state == IDLE) {
            if (radio_ready) {
                extern volatile unsigned et100_scan_status;
                if (scan_resume_pending && et100_scan_status == 0) scan_resume_pending = 0;
                else et100_scan_start();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (state == DRAIN) goto cleanup;
        uint32_t start = now_ms();
        if (manual) event("WS", "manual");
        while (state == WAIT_ADV && !matched && !cancel_requested &&
            (uint32_t)(now_ms() - start) < 90000) {
            if (radio_ready) et100_scan_start();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (!matched || cancel_requested) {
            event("CF", cancel_requested ? "cancelled" : "no_adv_timeout");
            goto cleanup;
        }
        event("CS", "");
        state = CONNECTING;
        scan_ended = 0; scan_stop_pending = 1;
        if (gap_scan_stop() != BLE_API_ERR_NO_ERR) {
            scan_stop_pending = 0; event("CF", "scan_stop"); goto cleanup;
        }
        scan_resume_pending = 1;
        start = now_ms();
        while (!scan_ended && !cancel_requested && (uint32_t)(now_ms()-start) < 1500)
            vTaskDelay(pdMS_TO_TICKS(5));
        if (!scan_ended || cancel_requested) { event("CF", "scan_stop"); goto cleanup; }
        gap_conn_param_t params = {0};
        params.peer_addr = peer;
        params.own_addr_type = GAP_ADDR_TYPE_STATIC;
        params.phy_mode = GAPM_PHY_TYPE_LE_1M;
        params.scan_intv = params.scan_window = 40;
        params.conn_intv_min = 12; params.conn_intv_max = 24;
        params.supervision_to = 500; params.ce_len_max = 2;
        initiating = 1; connection_end_seen = 0; connection_end_status = 0;
        if (gap_conn_start(&params) != BLE_API_ERR_NO_ERR) {
            initiating = 0; event("CF", "connect_start"); goto cleanup;
        }
        start = now_ms();
        while (!connected && !(connection_end_seen && connection_end_status != 0) && !cancel_requested &&
            (uint32_t)(now_ms()-start) < 10000) vTaskDelay(pdMS_TO_TICKS(5));
        if (!connected || cancel_requested) { event("CF", "connect_timeout"); goto cleanup; }
        state = SETUP;
        if (!setup()) { event("CF", "gatt_setup"); goto cleanup; }
        if (!authenticate()) { event("AF", "auth"); goto cleanup; }
        taskENTER_CRITICAL();
        memcpy(cache_mac, target, 6); eup_aiot_metadata_reset(&metadata);
        state = READY;
        taskEXIT_CRITICAL();
        event("AP", "");
        uint32_t activity = now_ms();
        uint8_t metadata_cmd = 1;
        while (connected && !cancel_requested && !output_fault &&
            (uint32_t)(now_ms()-activity) < 30000) {
            if (request_pending) {
                int ok = command(request.command, request.payload, request.length, 2000);
                if (ok) { activity = now_ms(); update_metadata_cache(request.command); }
                complete_request(ok);
            } else if (metadata_cmd <= 8) {
                uint8_t cmd = metadata_cmd++;
                if (command(cmd, NULL, 0, 1500)) {
                    update_metadata_cache(cmd);
                    activity = now_ms(); chunks(cmd, NULL, NULL);
                }
            } else vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (output_fault) event("CF", "uart_backpressure");
        else if (!cancel_requested && connected) event("TO", "idle");
cleanup:
        state = DRAIN;
        if (request_pending) complete_request(0);
        unsigned fault_reported = 0;
        uint32_t stop_attempt_ms = now_ms() - 1000;
        start = now_ms();
        /* Retain owner until the SDK confirms no initiator/link remains. */
        for (;;) {
            if (release_owner()) break;
            if ((uint32_t)(now_ms() - stop_attempt_ms) >= 1000) {
                if (initiating) (void)gap_conn_stop();
                if (connected) (void)gap_disconnect(conidx);
                stop_attempt_ms = now_ms();
            }
            if ((uint32_t)(now_ms()-start) >= 6000 && !fault_reported) {
                event("CF", "disconnect_drain_timeout"); fault_reported = 1;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (scan_resume_pending) et100_scan_start();
    }
}

void et100_gatt_init(int storage_ready)
{
    db_ready = storage_ready;
    uint8_t blob[100];
    if (db_ready && flashdb_get_length(AUTO_KEY) == sizeof(blob) &&
        flashdb_get(AUTO_KEY, blob, sizeof(blob)) == sizeof(blob) &&
        memcmp(blob, "EA\1\0", 4) == 0) {
        int valid = 1;
        for (unsigned i = 0; i < 48; i++) if ((unsigned)(blob[4+i]^blob[52+i]) != 255) valid = 0;
        if (valid) memcpy(auto_macs, blob + 4, sizeof(auto_macs));
    }
    output_queue = xQueueCreate(16, sizeof(struct queued_output));
    configASSERT(output_queue != NULL);
    BaseType_t created = xTaskCreate(worker, "gatt", 1536, NULL, 1, NULL);
    configASSERT(created == pdPASS);
}
void et100_gatt_add_profile(void)
{
    struct gatt_client client = {0};
    client.gatt_msg_handler = client_callback;
    client_id = gatt_add_client(&client);
}
unsigned et100_gatt_state(void) { return state == IDLE ? 0 : state == READY ? 2 : 1; }
unsigned et100_gatt_link_connected(void) { return connected; }
void et100_gatt_poll(void (*send)(const char *, size_t, void *), void *context)
{
    struct queued_output item;
    uart_task = xTaskGetCurrentTaskHandle(); uart_send = send; uart_context = context;
    while (xQueueReceive(output_queue, &item, 0) == pdPASS) {
        if (item.ready_only && (item.generation != session_generation ||
            state != READY || !connected || cancel_requested)) continue;
        send(item.line, strlen(item.line), context);
        break;
    }
}
void et100_gatt_gap(struct gap_event *message)
{
    if (!output_queue) return;
    taskENTER_CRITICAL();
    if (message->type == GAP_EVT_ADV_START && message->param.adv_start.status == 0) radio_ready = 1;
    if (message->type == GAP_EVT_SCAN_END) { scan_ended = 1; scan_stop_pending = 0; }
    if (message->type == GAP_EVT_CONN_END) {
        connection_end_seen = 1; connection_end_status = message->param.conn_end_status;
        /* A successful action can precede MASTER_CONNECT; retain its owner. */
        if (connection_end_status != 0 || connected) initiating = 0;
    }
    if (message->type == GAP_EVT_CONN_START && message->param.conn_start_status != 0) {
        initiating = 0; connection_end_seen = 1; connection_end_status = message->param.conn_start_status;
    }
    if (message->type == GAP_EVT_MASTER_CONNECT) {
        uint8_t mac[6];
        for (unsigned i = 0; i < 6; i++) mac[i] = message->param.connect.peer_addr.addr.addr[5-i];
        if (state == IDLE) {
            memcpy(target, mac, 6); session_generation++; state = DRAIN;
        } else if (memcmp(mac, target, 6)) cancel_requested = 1;
        connected = 1; initiating = 0;
        conidx = message->param.connect.conidx; connected_ms = now_ms();
    }
    if (message->type == GAP_EVT_DISCONNECT && connected && message->param.disconnect.conidx == conidx) {
        connected = 0;
        if (state == READY) state = DRAIN;
    }
    if (message->type == GATT_EVT_MTU && connected && message->param.mtu_ind.conidx == conidx) {
        mtu = message->param.mtu_ind.mtu; mtu_done = 1;
    }
    if (message->type == GAP_EVT_ADV_REPORT && message->param.adv_rpt) {
        struct gap_evt_adv_report *ad = message->param.adv_rpt;
        uint8_t mac[6];
        for (unsigned i = 0; i < 6; i++) mac[i] = ad->addr.addr.addr[5-i];
        if ((ad->evt_type == GAP_SCAN_EVT_CONN_UNDIR || ad->evt_type == GAP_SCAN_EVT_CONN_DIR) &&
            client_id != 0xff) {
            if (state == IDLE && (!cooldown_valid || (uint32_t)(now_ms()-cooldown_ms) >= 5000)) {
                for (unsigned i = 0; i < 8; i++) if (memcmp(auto_macs[i], mac, 6) == 0) {
                    memcpy(target, mac, 6); session_generation++; state = WAIT_ADV; manual = 0;
                    matched = cancel_requested = output_fault = 0; break;
                }
            }
            if (state == WAIT_ADV && memcmp(target, mac, 6) == 0) { peer = ad->addr; matched = 1; }
        }
    }
    taskEXIT_CRITICAL();
}

int et100_gatt_uart(const char *cmd, char *params, const char *token)
{
    char fields[160], original[112];
    uint8_t mac[6];
    snprintf(original, sizeof(original), "%s", params);
    if (!strcmp(cmd, "GTYPESWITCH")) {
        taskENTER_CRITICAL();
        if (state == IDLE) snprintf(fields, sizeof(fields), "0");
        else { char text[13]; mac_text(target,text); snprintf(fields,sizeof(fields),"1:%s",text); }
        taskEXIT_CRITICAL();
        reply(cmd, !*params, fields, token); return 1;
    }
    if (!strcmp(cmd, "STYPESWITCH")) {
        int valid = 0;
        if (!strcmp(params,"0") || !strcmp(params,"0;")) {
            cancel_requested = 1; valid = 1;
            snprintf(original, sizeof(original), "0");
        }
        else if (params[0] == '1' && (params[1] == ':' || params[1] == ';') &&
                 unhex(params+2,mac,6) == 6) {
            char text[13]; mac_text(mac, text);
            snprintf(original, sizeof(original), "1:%s", text);
            taskENTER_CRITICAL();
            if (state == IDLE && client_id != 0xff) {
                memcpy(target,mac,6); session_generation++; manual = 1; matched = cancel_requested = output_fault = 0;
                state = WAIT_ADV; valid = 1;
            }
            taskEXIT_CRITICAL();
        }
        reply(cmd,valid,original,token); return 1;
    }
    if (!strcmp(cmd,"GAUTOLIST")) {
        size_t used = 0; fields[0] = 0;
        taskENTER_CRITICAL();
        for (unsigned i=0;i<8;i++) {
            static const uint8_t zero[6];
            if (memcmp(auto_macs[i],zero,6)) {
                char text[13]; mac_text(auto_macs[i],text);
                used += snprintf(fields+used,sizeof(fields)-used,"%s%s",used?";":"",text);
            }
        }
        taskEXIT_CRITICAL();
        reply(cmd,db_ready && !*params,fields,token); return 1;
    }
    if (!strcmp(cmd,"SAUTOGATT")) {
        char *flag = strchr(params,';'); int valid = 0;
        if (flag) { *flag++=0;
            if (unhex(params,mac,6)==6 && (!strcmp(flag,"0") || !strcmp(flag,"1"))) {
                static const uint8_t zero[6];
                if (memcmp(mac,zero,6)) valid=save_auto(mac,flag[0]=='1');
            }
        }
        reply(cmd,valid,original,token); return 1;
    }
    if (!strcmp(cmd,"GSENSORINFO")) {
        int valid=0; snprintf(fields,sizeof(fields),"%s;;;;;",params);
        if (unhex(params,mac,6)==6) {
            struct eup_gatt_metadata copy; uint32_t age;
            taskENTER_CRITICAL();
            copy=metadata; age=(now_ms()-cache_ms)/1000;
            valid=copy.seen && memcmp(mac,cache_mac,6)==0;
            taskEXIT_CRITICAL();
            if (valid) {
                char battery[3]="",caps[9]="";
                if (copy.battery_seen) snprintf(battery,sizeof(battery),"%02X",copy.battery);
                if (copy.capabilities_seen) snprintf(caps,sizeof(caps),"%08lX",(unsigned long)copy.capabilities);
                snprintf(fields,sizeof(fields),"%s;%u;%s;%s;%s;%lu",params,auto_enabled(mac),
                    copy.fw_version,battery,caps,(unsigned long)age);
            }
        }
        reply(cmd,valid,fields,token); return 1;
    }
    if (strcmp(cmd,"GSENSORCMD") && strcmp(cmd,"SSENSORCFG")) return 0;
    char *value=strchr(params,';'), *hex=NULL;
    uint8_t operation, payload[32]; size_t length=0;
    int valid=0;
    if (value) { *value++=0; hex=strchr(value,';'); if(hex) *hex++=0; }
    if (value && unhex(params,mac,6)==6 && unhex(value,&operation,1)==1) {
        if (!strcmp(cmd,"GSENSORCMD") && !hex) valid=1;
        if (!strcmp(cmd,"SSENSORCFG") && hex) {
            int n=unhex(hex,payload+2,30);
            if(n>0) { payload[0]=operation; payload[1]=(uint8_t)n; length=(size_t)n+2; operation=0x12; valid=1; }
        }
    }
    taskENTER_CRITICAL();
    valid=valid && state==READY && connected && !cancel_requested && !request_pending && memcmp(mac,target,6)==0;
    if(valid) {
        request.command=operation; request.length=length;
        if(length) memcpy(request.payload,payload,length);
        snprintf(request.name,sizeof(request.name),"%s",cmd);
        snprintf(request.fields,sizeof(request.fields),"%s",original);
        snprintf(request.token,sizeof(request.token),"%s",token);
        request_pending=1;
    }
    taskEXIT_CRITICAL();
    if(!valid) reply(cmd,0,!strcmp(cmd,"GSENSORCMD")?"no_session":original,token);
    return 1;
}
