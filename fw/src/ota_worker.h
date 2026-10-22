#ifndef ET100_OTA_WORKER_H
#define ET100_OTA_WORKER_H
/* Single OTA owner task (v0.8.6): every SMP request, from UART3 or BLE, is
 * processed here so BLE callbacks never block and both transports share one
 * OTA manager. Each transport has one static request slot (SMP is
 * request/response), so no heap queue is needed. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum ota_transport { OTA_TRANSPORT_SERIAL, OTA_TRANSPORT_BLE, OTA_TRANSPORT_COUNT };
/* tag: value given at submit (BLE: connection generation); the sender drops the
 * response if the transport state no longer matches it. */
typedef void (*ota_worker_send_fn)(const uint8_t *packet, size_t length, uint32_t tag);

/* Create the task. send[] delivers a response packet back on each transport. */
void ota_worker_start(const ota_worker_send_fn send[OTA_TRANSPORT_COUNT]);
/* Copy one complete SMP request into the transport's slot. False if the slot
 * is still busy (client sent before receiving the previous response). */
bool ota_worker_submit(enum ota_transport transport, const uint8_t *packet, size_t length, uint32_t tag);
/* Lowest observed free bytes in the worker stack (0xA5 fill scan). */
extern volatile uint32_t et100_ota_stack_free;
/* True while any transport exchanged SMP within the last 30 s. */
bool ota_worker_active(void);
#endif
