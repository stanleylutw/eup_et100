#include "ota_worker.h"
#include "ota_manager.h"
#include "smp_server.h"
#include "smp_serial.h"
#include "nor_qspi1.h"
#include "boot_health.h"
#include "fr30xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fdb_app.h"
#include "et100_fdb_keys.h"
#include "system_status.h"
#include <string.h>

#define REPORT_QUIET_MS 3000u

struct slot {
    volatile bool full;
    size_t length;
    uint32_t tag;
    uint8_t data[SMP_SERIAL_MAX_PACKET];
};
static struct slot slots[OTA_TRANSPORT_COUNT];
static ota_worker_send_fn senders[OTA_TRANSPORT_COUNT];
static TaskHandle_t worker;
static struct ota_manager ota;
static struct ota_env env;
static struct smp_ops ops;
static uint8_t response[SMP_SERIAL_MAX_PACKET];
static volatile TickType_t last_activity;
static volatile bool seen;
static bool smp_in_flight, upload_open;
static bool response_in_flight;
static uint32_t upload_changed_ms;
static struct sensor_store sensor;
static volatile bool delete_queued;
static volatile int delete_result = -1;
static struct inst_record inst_cached,inst_queued;
static bool inst_valid;
static volatile bool inst_pending;
bool ota_worker_inst_record(struct inst_record *out)
{
    taskENTER_CRITICAL();*out=inst_cached;bool valid=inst_valid;taskEXIT_CRITICAL();
    return valid;
}
bool ota_worker_inst_save(const struct inst_record *r)
{
    taskENTER_CRITICAL();
    if (!worker || inst_pending) { taskEXIT_CRITICAL();return false; }
    inst_queued=*r;__DMB();inst_pending=true;
    taskEXIT_CRITICAL();xTaskNotifyGive(worker);return true;
}
bool ota_worker_inst_pending(void) { return inst_pending; }
static struct {
    volatile bool pending;
    volatile int rc;
    uint32_t offset;
    void *data;
    size_t length;
} read_request;

static size_t pkg_load(uint8_t *record, size_t capacity)
{
    size_t n = flashdb_get_length(ET100_FDB_SENSOR_PKG_KEY);
    if (!n || n > capacity) return n;
    return flashdb_get(ET100_FDB_SENSOR_PKG_KEY, record, (uint32_t)n);
}
static int pkg_save(const uint8_t *record, size_t length)
{
    static uint8_t verify[SENSOR_STORE_RECORD_SIZE];
    if (length > sizeof verify ||
        flashdb_set(ET100_FDB_SENSOR_PKG_KEY, (uint8_t *)record, (uint32_t)length) != FDB_NO_ERR ||
        flashdb_get(ET100_FDB_SENSOR_PKG_KEY, verify, (uint32_t)length) != length)
        return -1;
    return memcmp(record, verify, length) ? -1 : 0;
}
static int pkg_remove(void)
{
    (void)flashdb_del(ET100_FDB_SENSOR_PKG_KEY);
    return flashdb_get_length(ET100_FDB_SENSOR_PKG_KEY) ? -1 : 0;
}
static const struct sensor_store_ops pkg_ops = { pkg_load, pkg_save, pkg_remove };

void nor_qspi1_yield(void) { taskYIELD(); }

static int app_nor_prepare(void)
{
    struct nor_port *nor = nor_qspi1_port();
    int rc = nor->init();
    if (!rc) rc=nor_qspi1_authorize_writes();
    et100_sys_set(SYS_NOR,rc ? 5 : 2);
    return rc;
}
static void lock(void) { taskENTER_CRITICAL(); }
static void unlock(void) { taskEXIT_CRITICAL(); }
static uint32_t now_ms(void) { return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS; }

/* Only the worker reads manager internals; readers use this published snapshot. */
static void publish_upload(void)
{
    taskENTER_CRITICAL();
    upload_open = ota_manager_upload_open(&ota);
    upload_changed_ms = ota.owner_ms;
    taskEXIT_CRITICAL();
}

void ota_worker_smp_activity(void)
{
    taskENTER_CRITICAL();
    last_activity = xTaskGetTickCount();
    seen = true;
    taskEXIT_CRITICAL();
}

bool ota_worker_active(void)
{
    taskENTER_CRITICAL();
    bool active = smp_in_flight || response_in_flight ||
        (seen && (TickType_t)(xTaskGetTickCount() - last_activity) < pdMS_TO_TICKS(REPORT_QUIET_MS));
    for (unsigned t = 0; t < OTA_TRANSPORT_COUNT; t++) active |= slots[t].full;
    taskEXIT_CRITICAL();
    return active;
}

bool ota_worker_sensor_status(struct sensor_store_status *out)
{
    if (!worker || !ota_manager_sensor_supported(&ota)) return false;
    ota_manager_sensor_status(&ota, out);
    return true;
}

bool ota_worker_sensor_delete(void)
{
    if (!worker || delete_queued) return false;
    delete_result = -1;
    __DMB();
    delete_queued = true;
    xTaskNotifyGive(worker);
    return true;
}

int ota_worker_sensor_deleted(void)
{
    int rc = delete_result;
    if (rc >= 0) delete_result = -1;
    return rc;
}

bool ota_worker_relay_begin(void)
{
    bool ok = false;
    if (!worker || !ota_manager_sensor_supported(&ota)) return false;
    taskENTER_CRITICAL();
    bool pending = smp_in_flight;
    for (unsigned t = 0; t < OTA_TRANSPORT_COUNT; t++) pending |= slots[t].full;
    bool held = upload_open && (uint32_t)(now_ms() - upload_changed_ms) < OTA_OWNER_HOLD_MS;
    if (!delete_queued && !pending && !held && !ota.relay) {
        ota_manager_set_relay(&ota, true);
        ok = true;
    }
    taskEXIT_CRITICAL();
    return ok;
}

void ota_worker_relay_end(void)
{
    ota_manager_set_relay(&ota, false);
}

int ota_worker_sensor_read(uint32_t offset, void *data, size_t length)
{
    if (!worker || read_request.pending) return -1;
    read_request.offset = offset;
    read_request.data = data;
    read_request.length = length;
    read_request.rc = -1;
    __DMB();
    read_request.pending = true;
    xTaskNotifyGive(worker);
    for (unsigned waited = 0; read_request.pending; waited++) {
        if (waited >= 2000) return -1;   /* the worker clears pending; buffer stays owned */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return read_request.rc;
}

bool ota_worker_submit(enum ota_transport transport, const uint8_t *packet, size_t length, uint32_t tag)
{
    if (transport >= OTA_TRANSPORT_COUNT || !worker || length > SMP_SERIAL_MAX_PACKET) return false;
    struct slot *s = &slots[transport];
    if (s->full) return false;
    memcpy(s->data, packet, length);
    s->length = length;
    s->tag = tag;
    __DMB();
    s->full = true;
    ota_worker_smp_activity();
    xTaskNotifyGive(worker);
    return true;
}

/* Stack high-water without INCLUDE_uxTaskGetStackHighWaterMark (SDK config):
 * FreeRTOS pre-fills stacks with 0xA5 because configCHECK_FOR_STACK_OVERFLOW
 * is 2. Scan upward from just above the stack base (estimated from a local in
 * the task's first frame) for the first used word. Conservative: a lower bound. */
#define WORKER_STACK_WORDS 1024u
#define SCAN_MARGIN 128u
volatile uint32_t et100_ota_stack_free = UINT32_MAX;
static const uint8_t *stack_low;
static void stack_check(void)
{
    if (!stack_low) return;
    const uint32_t *p = (const uint32_t *)stack_low;
    uint32_t free_words = 0;
    while (p[free_words] == 0xa5a5a5a5u) free_words++;
    uint32_t bytes = free_words * 4u + SCAN_MARGIN;
    if (bytes < et100_ota_stack_free) et100_ota_stack_free = bytes;
}

static void worker_task(void *argument)
{
    (void)argument;
    volatile uint32_t top_marker = 0;
    stack_low = (const uint8_t *)(((uintptr_t)&top_marker & ~3u) - WORKER_STACK_WORDS * 4u + SCAN_MARGIN);
    struct inst_record loaded;
    bool valid=!app_nor_prepare() && inst_record_read(env.nor,&loaded);
    taskENTER_CRITICAL();
    if (valid) inst_cached=loaded;
    inst_valid=valid;taskEXIT_CRITICAL();
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (inst_pending) {
            struct inst_record update=inst_queued;
            bool saved=!app_nor_prepare() && !inst_record_write(env.nor,&update);
            taskENTER_CRITICAL();
            if (saved) { inst_cached=update;inst_valid=true; }
            inst_pending=false;taskEXIT_CRITICAL();
            stack_check();
        }
        if (read_request.pending) {
            read_request.rc = ota_manager_sensor_read(&ota, read_request.offset, read_request.data,
                                                      read_request.length) ? -1 : 0;
            __DMB();
            read_request.pending = false;
        }
        if (delete_queued) {
            ota_manager_set_source(&ota, OTA_RS232);
            int rc = ota_manager_sensor_delete(&ota);
            delete_result = rc < 0 ? SMP_EUNKNOWN : rc;
            publish_upload();
            __DMB();
            delete_queued = false;
            stack_check();
        }
        for (unsigned t = 0; t < OTA_TRANSPORT_COUNT; t++) {
            struct slot *s = &slots[t];
            if (!s->full) continue;
            taskENTER_CRITICAL();smp_in_flight = true;taskEXIT_CRITICAL();
            bool reset = false;
            ota_manager_set_source(&ota, t == OTA_TRANSPORT_BLE ? OTA_BLE : OTA_RS232);
            size_t n = smp_server_process(&ops, s->data, s->length, response, sizeof response, &reset);
            et100_boot_refresh();
            publish_upload();
            taskENTER_CRITICAL();
            response_in_flight = true;
            s->full = false;
            smp_in_flight = false;
            taskEXIT_CRITICAL();
            if (n && senders[t]) senders[t](response, n, s->tag);
            /* Keep the report gate across every line/notification in the reply. */
            taskENTER_CRITICAL();
            last_activity = xTaskGetTickCount();
            response_in_flight = false;
            taskEXIT_CRITICAL();
            stack_check();
            if (reset) {
                vTaskDelay(pdMS_TO_TICKS(100));   /* last UART byte / BLE notify out */
                NVIC_SystemReset();
            }
        }
    }
}

void ota_worker_start(const ota_worker_send_fn send[OTA_TRANSPORT_COUNT], bool storage_ready)
{
    for (unsigned t = 0; t < OTA_TRANSPORT_COUNT; t++) senders[t] = send[t];
    env.nor = nor_qspi1_port();
    env.nor_prepare = app_nor_prepare;
    env.internal = et100_boot_port();
    env.app_slot = (const uint8_t *)ET100_APP_SLOT;
    env.lock = lock;
    env.unlock = unlock;
    env.source = OTA_RS232;
    env.now_ms = now_ms;
    if (storage_ready) {
        sensor_store_init(&sensor, &pkg_ops);
        env.sensor = &sensor;
    }
    ota_manager_init(&ota, &env);
    ota_manager_smp_ops(&ota, &ops);
    /* Same priority as the serial task: long hashes are time-sliced, and the
     * serial task keeps feeding the watchdog. 4 KB stack from the 50 KB heap. */
    BaseType_t result = xTaskCreate(worker_task, "ota", WORKER_STACK_WORDS, NULL, 1, &worker);
    configASSERT(result == pdPASS);
}
