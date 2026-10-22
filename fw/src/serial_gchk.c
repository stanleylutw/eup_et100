#include "serial_gchk.h"
#include "../include/eup_fw_info.h"
#include <stdio.h>
#include <string.h>
#ifndef ET100_SERIAL_HOST_TEST
#include "gatt_client.h"
#endif

static unsigned current_gatt_state(void)
{
#ifndef ET100_SERIAL_HOST_TEST
    return et100_gatt_state();
#else
    return 0;
#endif
}

static int ascii_alnum(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

void et100_serial_reset(struct et100_serial_parser *p)
{
    p->length = 0;
    p->discard = 0;
    p->cr = 0;
}

void et100_serial_init(struct et100_serial_parser *p, const char *identity,
                       et100_serial_send_fn send, void *context)
{
    memset(p, 0, sizeof(*p));
    snprintf(p->identity, sizeof(p->identity), "%s", identity);
    p->send = send;
    p->context = context;
    p->gtime = ET100_GTIME_DEFAULT;
    p->cids[0] = 0x0639;
}

void et100_serial_set_cid_storage(struct et100_serial_parser *p,
                                  const uint16_t cids[4], et100_serial_save_cids_fn save)
{
    if (cids != p->cids) memcpy(p->cids, cids, sizeof(p->cids));
    p->save_cids = save;
}

void et100_serial_set_storage(struct et100_serial_parser *p, int ready,
                             uint16_t seconds, et100_serial_save_fn save,
                             void *context)
{
    p->storage_ready = ready != 0;
    p->gtime = seconds >= 1 && seconds <= ET100_GTIME_MAX ?
               seconds : ET100_GTIME_DEFAULT;
    p->save = save;
    p->storage_context = context;
}

static int parse_seconds(const char *text, uint16_t *result)
{
    unsigned value = 0;
    if (!*text)
        return 0;
    for (; *text; text++) {
        if (*text < '0' || *text > '9')
            return 0;
        value = value * 10 + (unsigned)(*text - '0');
        if (value > ET100_GTIME_MAX)
            return 0;
    }
    *result = (uint16_t)value;
    return value >= 1;
}

static int parse_cids(char *params, uint16_t cids[4])
{
    memset(cids, 0, 4 * sizeof(*cids));
    for (unsigned i = 0; i < 4; i++) {
        char *end = strchr(params, ';');
        if ((i < 3 && !end) || (i == 3 && end)) return 0;
        if (end) *end = '\0';
        size_t n = strlen(params);
        if (n && n != 4) return 0;
        for (size_t j = 0; j < n; j++) {
            unsigned char c = (unsigned char)params[j];
            unsigned value;
            if (c >= '0' && c <= '9') value = c - '0';
            else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
            else return 0;
            cids[i] = (uint16_t)((cids[i] << 4) | value);
        }
        if (end) params = end + 1;
    }
    return 1;
}

static int cid_response(char *out, size_t capacity, const char *command, int valid,
                         const uint16_t cids[4], const char *token)
{
    char text[4][5] = {{0}};
    for (unsigned i = 0; i < 4; i++)
        if (cids[i]) snprintf(text[i], sizeof(text[i]), "%04X", cids[i]);
    return snprintf(out, capacity, "AT$PD=BLEGW$%s;%u;%s;%s;%s;%s;%s\r\n",
                    command, valid, text[0], text[1], text[2], text[3], token);
}

static void handle_line(struct et100_serial_parser *p)
{
    char response[160];
    char *command, *end, *token;
    size_t command_length;
    int length;

    p->line[p->length] = '\0';
    if (strncmp(p->line, "BLEGW$", 6) != 0)
        return;
    command = p->line + 6;
    end = strchr(command, ';');
    token = strrchr(command, ';');
    if (!end || !token || strlen(++token) != 4)
        return;
    for (size_t i = 0; i < 4; i++)
        if (!ascii_alnum((unsigned char)token[i]))
            return;
    command_length = (size_t)(end - command);
    if (command_length == 0 || command_length > 24)
        return;
    for (size_t i = 0; i < command_length; i++)
        if (!((command[i] >= 'A' && command[i] <= 'Z') ||
              (command[i] >= '0' && command[i] <= '9')))
            return;
    *end = '\0';
    if (strcmp(command, "GCHK") == 0 && token == end + 1) {
        length = snprintf(response, sizeof(response),
            "AT$PD=BLEGW$GCHK;1;%s;Freqchip;DEV1.4_fr3068ep-c;" EUP_FIRMWARE_VERSION ";%u;%s\r\n",
            p->identity, current_gatt_state(), token);
    } else if (strcmp(command, "GGTIME") == 0 && token == end + 1) {
        length = snprintf(response, sizeof(response),
                          "AT$PD=BLEGW$GGTIME;%u;%u;%s\r\n",
                          p->storage_ready, p->gtime, token);
    } else if (strcmp(command, "SGTIME") == 0) {
        uint16_t seconds;
        int valid = 0;
        char *params = end + 1;
        if (token != params)
            token[-1] = '\0';
        else
            params = "";
        if (parse_seconds(params, &seconds) && p->storage_ready && p->save &&
            p->save(seconds, p->storage_context) == 0) {
            p->gtime = seconds;
            valid = 1;
        }
        length = snprintf(response, sizeof(response),
                          "AT$PD=BLEGW$SGTIME;%u;%s;%s\r\n",
                          valid, params, token);
    } else if (strcmp(command, "OTABAUD") == 0) {
        char *params = end + 1;
        unsigned long baud = 0;
        int valid = 0;
        if (token != params) {
            token[-1] = '\0';
            if (!strcmp(params, "9600")) baud = 9600;
            else if (!strcmp(params, "115200")) baud = 115200;
        } else {
            params = "";
        }
        if (baud && p->baud_supported) {
            valid = 1;
            p->baud_request = (uint32_t)baud;
        }
        length = snprintf(response, sizeof(response),
                          "AT$PD=BLEGW$OTABAUD;%u;%s;%s\r\n", valid, params, token);
    } else if (strcmp(command, "GCID") == 0 && token == end + 1) {
        length = cid_response(response, sizeof(response), command,
                              p->storage_ready, p->cids, token);
    } else if (strcmp(command, "SCID") == 0) {
        uint16_t cids[4] = {0};
        int valid = 0, parsed = 0;
        if (token != end + 1) {
            token[-1] = '\0';
            parsed = parse_cids(end + 1, cids);
            valid = parsed && p->storage_ready && p->save_cids &&
                    p->save_cids(cids, p->storage_context) == 0;
        }
        if (valid) {
            memcpy(p->cids, cids, sizeof(p->cids));
            p->cid_revision++;
        }
        if (!parsed) memset(cids, 0, sizeof(cids));
        length = cid_response(response, sizeof(response), command, valid, cids, token);
    } else {
#ifndef ET100_SERIAL_HOST_TEST
        char *params = end + 1;
        if (token != params) token[-1] = '\0';
        else params = "";
        if (et100_gatt_uart(command, params, token)) return;
#endif
        length = snprintf(response, sizeof(response),
                          "AT$PD=BLEGW$%s;0;%s\r\n", command, token);
    }
    if (length > 0 && (size_t)length < sizeof(response))
        p->send(response, (size_t)length, p->context);
}

void et100_serial_feed(struct et100_serial_parser *p, uint8_t byte)
{
    if (byte == '\n') {
        if (p->cr && !p->discard && p->length)
            handle_line(p);
        et100_serial_reset(p);
        return;
    }
    if (p->discard)
        return;
    if (p->cr) {
        p->discard = 1;
        return;
    }
    if (byte == '\r') {
        p->cr = 1;
        return;
    }
    if (byte < 0x20 || byte > 0x7e || p->length >= sizeof(p->line) - 1) {
        p->discard = 1;
        return;
    }
    p->line[p->length++] = (char)byte;
}

#ifndef ET100_SERIAL_HOST_TEST
#include "fr30xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_at.h"
#include "fdb_app.h"
#include "ble_reporter.h"
#include "app_gatt_server.h"
#include "boot_health.h"
#include "smp_serial.h"
#include "smp_server.h"
#include "ota_worker.h"
#include "semphr.h"
#include "smp_ble.h"

/* Reserved user key; distinct from SDK random seed and BT keys. */
#define ET100_FDB_GTIME_KEY 0x00030002U
#define ET100_FDB_CID_KEY 0x00030003U
static int storage_initialized;

void et100_serial_storage_ready(int ready)
{
    storage_initialized = ready;
}

static int record_read(uint8_t record[8])
{
    return flashdb_get_length(ET100_FDB_GTIME_KEY) == 8 &&
           flashdb_get(ET100_FDB_GTIME_KEY, record, 8) == 8;
}

static uint16_t load_gtime(void)
{
    uint8_t record[8];
    if (record_read(record) && record[0] == 'E' && record[1] == 'T' &&
        record[2] == 1 && record[3] == 0 &&
        (unsigned)(record[4] ^ record[6]) == 255 &&
        (unsigned)(record[5] ^ record[7]) == 255) {
        unsigned seconds = record[4] | ((unsigned)record[5] << 8);
        if (seconds >= 1 && seconds <= ET100_GTIME_MAX)
            return (uint16_t)seconds;
    }
    return ET100_GTIME_DEFAULT;
}

static int save_gtime(uint16_t seconds, void *context)
{
    uint8_t record[8] = {'E', 'T', 1, 0, (uint8_t)seconds,
                         (uint8_t)(seconds >> 8), 0, 0};
    uint8_t verify[8];
    (void)context;
    record[6] = (uint8_t)~record[4];
    record[7] = (uint8_t)~record[5];
    /* Avoid flash wear when the durable record already contains this value. */
    if (record_read(verify) && memcmp(record, verify, 8) == 0)
        return 0;
    if (flashdb_set(ET100_FDB_GTIME_KEY, record, 8) != FDB_NO_ERR)
        return -1;
    return record_read(verify) && memcmp(record, verify, 8) == 0 ? 0 : -1;
}

static int cid_record_read(uint8_t record[20])
{
    return flashdb_get_length(ET100_FDB_CID_KEY) == 20 &&
           flashdb_get(ET100_FDB_CID_KEY, record, 20) == 20;
}

static void load_cids(uint16_t cids[4])
{
    uint8_t record[20];
    if (!cid_record_read(record) || record[0] != 'E' || record[1] != 'C' ||
        record[2] != 1 || record[3] != 0) return;
    for (unsigned i = 0; i < 8; i++)
        if ((unsigned)(record[4 + i] ^ record[12 + i]) != 255) return;
    for (unsigned i = 0; i < 4; i++)
        cids[i] = record[4 + 2*i] | ((uint16_t)record[5 + 2*i] << 8);
}

static int save_cids(const uint16_t cids[4], void *context)
{
    uint8_t record[20] = {'E', 'C', 1, 0};
    uint8_t verify[20];
    (void)context;
    for (unsigned i = 0; i < 4; i++) {
        record[4 + 2*i] = (uint8_t)cids[i];
        record[5 + 2*i] = (uint8_t)(cids[i] >> 8);
    }
    for (unsigned i = 0; i < 8; i++) record[12 + i] = (uint8_t)~record[4 + i];
    if (cid_record_read(verify) && memcmp(record, verify, 20) == 0) return 0;
    if (flashdb_set(ET100_FDB_CID_KEY, record, 20) != FDB_NO_ERR) return -1;
    return cid_record_read(verify) && memcmp(record, verify, 20) == 0 ? 0 : -1;
}

/* 2 KB holds a full SMP request burst (~1.4 KB encoded) between task polls. */
#define RX_RING_SIZE 2048
#define SMP_IDLE_MS 30000
static uint8_t rx_byte;
static volatile uint16_t rx_head, rx_tail;
static volatile unsigned char rx_overflow;
static uint8_t rx_ring[RX_RING_SIZE];
static UART_HandleTypeDef *serial_uart;
static struct et100_serial_parser serial_parser;

static SemaphoreHandle_t tx_lock;   /* serial task and OTA worker both transmit */

static void serial_send(const char *data, size_t length, void *context)
{
    UART_HandleTypeDef *uart = context;
    if (tx_lock) xSemaphoreTake(tx_lock, portMAX_DELAY);
    uart_transmit(uart, (void *)data, length);
    while (!uart->UARTx->USR.TFE) {}
    if (tx_lock) xSemaphoreGive(tx_lock);
}

/* ---- SMP over serial: framing here, processing in the OTA worker (v0.8.6) ---- */
static struct smp_serial smp_rx;
static TickType_t smp_last;
static int smp_seen;
static uint32_t uart_baud = 9600;
static TickType_t baud_since;

static void smp_line_out(const char *data, size_t length, void *context)
{
    serial_send(data, length, context);
}

/* Worker -> UART3: encode one response packet into SMP serial lines. */
static void smp_serial_respond(const uint8_t *packet, size_t length)
{
    smp_serial_encode(packet, length, smp_line_out, serial_uart);
}

static void set_baud(uint32_t baud)
{
    vTaskDelay(pdMS_TO_TICKS(5));        /* response fully shifted out at the old rate */
    taskENTER_CRITICAL();
    serial_uart->Init.BaudRate = baud;
    uart_init(serial_uart);
    uart_receive_IT(serial_uart, &rx_byte, 1);
    taskEXIT_CRITICAL();
    uart_baud = baud;
    baud_since = xTaskGetTickCount();
}

static void serial_task(void *argument)
{
    uint8_t address[] = {0x32, 0x32, 0x32, 0x32, 0x32, 0xc0};
    uint8_t id_length = 5;
    char identity[13];
    (void)argument;

    /* Match app_btdm_init's unique-ID-based random static BLE identity. */
    system_get_unique_ID(address, &id_length);
    address[5] = 0xc0;
    snprintf(identity, sizeof(identity), "%02X%02X%02X%02X%02X%02X",
             address[5], address[4], address[3], address[2], address[1], address[0]);
    et100_serial_init(&serial_parser, identity, serial_send, serial_uart);
    serial_parser.baud_supported = 1;
    smp_serial_init(&smp_rx);
    et100_serial_set_storage(&serial_parser, storage_initialized,
        storage_initialized ? load_gtime() : ET100_GTIME_DEFAULT,
        save_gtime, NULL);
    if (storage_initialized) load_cids(serial_parser.cids);
    et100_serial_set_cid_storage(&serial_parser, serial_parser.cids, save_cids);
    et100_scan_configure(serial_parser.cids, serial_parser.gtime, 1);
    unsigned cid_revision = serial_parser.cid_revision;
    uart_receive_IT(serial_uart, &rx_byte, 1);
    et100_gatt_poll(serial_send, serial_uart);

    for (;;) {
        et100_boot_poll(storage_initialized);
        unsigned received = 0;
        /* Drain queued input before a potentially 200-ms UART transmission. */
        for (; received < RX_RING_SIZE; received++) {
            int value = -1;
            taskENTER_CRITICAL();
            if (rx_overflow) {
                rx_tail = rx_head;
                rx_overflow = 0;
                et100_serial_reset(&serial_parser);
                serial_parser.discard = 1;
            }
            if (rx_tail != rx_head) {
                value = rx_ring[rx_tail];
                rx_tail = (rx_tail + 1) % RX_RING_SIZE;
            }
            taskEXIT_CRITICAL();
            if (value < 0) break;
            const uint8_t *packet;
            size_t packet_length;
            int smp = smp_serial_feed(&smp_rx, (uint8_t)value, &packet, &packet_length);
            if (smp) {
                smp_last = xTaskGetTickCount();
                smp_seen = 1;
                if (smp == 2) (void)ota_worker_submit(OTA_TRANSPORT_SERIAL, packet, packet_length);
                continue;
            }
            et100_serial_feed(&serial_parser, (uint8_t)value);
            if (serial_parser.baud_request) {
                uint32_t baud = serial_parser.baud_request;
                serial_parser.baud_request = 0;
                if (baud != uart_baud) set_baud(baud);
            }
            if (value == '\n') {
                et100_scan_configure(serial_parser.cids, serial_parser.gtime,
                                  cid_revision != serial_parser.cid_revision);
                cid_revision = serial_parser.cid_revision;
            }
        }
        et100_gatt_poll(serial_send, serial_uart);
        et100_app_server_poll();
        TickType_t now = xTaskGetTickCount();
        int smp_active = smp_seen && (TickType_t)(now - smp_last) < pdMS_TO_TICKS(SMP_IDLE_MS);
        /* Pause unsolicited Type-1 reports while any SMP client (UART or BLE) is active. */
        if (!smp_active && !ota_worker_active()) et100_scan_poll(serial_send, serial_uart);
        /* OTABAUD fallback: return to 9600 after 30 s without SMP traffic. */
        if (uart_baud != 9600 && !smp_active &&
            (TickType_t)(now - baud_since) >= pdMS_TO_TICKS(SMP_IDLE_MS))
            set_baud(9600);
        if (!received)
            vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_at_rx_done(struct __UART_HandleTypeDef *handle)
{
    uint16_t next = (rx_head + 1) % RX_RING_SIZE;
    if (next == rx_tail)
        rx_overflow = 1;
    else {
        rx_ring[rx_head] = rx_byte;
        rx_head = next;
    }
    uart_receive_IT(handle, &rx_byte, 1);
}

void app_at_init(struct __UART_HandleTypeDef *handle)
{
    serial_uart = handle;
    tx_lock = xSemaphoreCreateMutex();
    configASSERT(tx_lock);
    const ota_worker_send_fn senders[OTA_TRANSPORT_COUNT] = {
        [OTA_TRANSPORT_SERIAL] = smp_serial_respond,
#if defined(ET100_SMP_BLE_OPEN) && ET100_SMP_BLE_OPEN
        [OTA_TRANSPORT_BLE] = smp_ble_respond,
#endif
    };
    ota_worker_start(senders);
    et100_gatt_init(storage_initialized);
    BaseType_t result = xTaskCreate(serial_task, "serial", 1024, NULL, 1, NULL);
    configASSERT(result == pdPASS);
}

/* Legacy AT events are not produced by the serial-only transport. */
void app_at_cmd_recv_handler(uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
}
#endif
