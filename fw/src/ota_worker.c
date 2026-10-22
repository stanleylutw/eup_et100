#include "ota_worker.h"
#include "ota_manager.h"
#include "smp_server.h"
#include "smp_serial.h"
#include "nor_qspi1.h"
#include "boot_health.h"
#include "fr30xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define ACTIVE_MS 30000u

struct slot {
    volatile bool full;
    size_t length;
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

void nor_qspi1_yield(void) { taskYIELD(); }

static int app_nor_prepare(void)
{
    struct nor_port *nor = nor_qspi1_port();
    int rc = nor->init();
    return rc ? rc : nor_qspi1_authorize_writes();
}
static void lock(void) { taskENTER_CRITICAL(); }
static void unlock(void) { taskEXIT_CRITICAL(); }
static uint32_t now_ms(void) { return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS; }

bool ota_worker_active(void)
{
    return seen && (TickType_t)(xTaskGetTickCount() - last_activity) < pdMS_TO_TICKS(ACTIVE_MS);
}

bool ota_worker_submit(enum ota_transport transport, const uint8_t *packet, size_t length)
{
    if (transport >= OTA_TRANSPORT_COUNT || !worker || length > SMP_SERIAL_MAX_PACKET) return false;
    struct slot *s = &slots[transport];
    if (s->full) return false;
    memcpy(s->data, packet, length);
    s->length = length;
    __DMB();
    s->full = true;
    last_activity = xTaskGetTickCount();
    seen = true;
    xTaskNotifyGive(worker);
    return true;
}

static void worker_task(void *argument)
{
    (void)argument;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (unsigned t = 0; t < OTA_TRANSPORT_COUNT; t++) {
            struct slot *s = &slots[t];
            if (!s->full) continue;
            bool reset = false;
            ota_manager_set_source(&ota, t == OTA_TRANSPORT_BLE ? OTA_BLE : OTA_RS232);
            size_t n = smp_server_process(&ops, s->data, s->length, response, sizeof response, &reset);
            s->full = false;
            last_activity = xTaskGetTickCount();
            if (n && senders[t]) senders[t](response, n);
            if (reset) {
                vTaskDelay(pdMS_TO_TICKS(100));   /* last UART byte / BLE notify out */
                NVIC_SystemReset();
            }
        }
    }
}

void ota_worker_start(const ota_worker_send_fn send[OTA_TRANSPORT_COUNT])
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
    ota_manager_init(&ota, &env);
    ota_manager_smp_ops(&ota, &ops);
    /* Same priority as the serial task: long hashes are time-sliced, and the
     * serial task keeps feeding the watchdog. 4 KB stack from the 50 KB heap. */
    BaseType_t result = xTaskCreate(worker_task, "ota", 1024, NULL, 1, &worker);
    configASSERT(result == pdPASS);
}
