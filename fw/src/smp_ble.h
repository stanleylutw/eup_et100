#ifndef ET100_SMP_BLE_H
#define ET100_SMP_BLE_H
/* SMP over BLE GATT (Nordic MCUmgr service), v0.8.6. Registered only when
 * ET100_SMP_BLE_OPEN is defined (development builds; no authentication). */
#include <stddef.h>
#include <stdint.h>

/* Service 8D53DC1D-1DB7-4CD3-868B-8A527460AA84, characteristic
 * DA2E7828-FBCE-4E01-AE9E-261174997C48 (Bluetooth little-endian byte order). */
#define SMP_BLE_SERVICE_UUID {0x84,0xaa,0x60,0x74,0x52,0x8a,0x8b,0x86,0xd3,0x4c,0xb7,0x1d,0x1d,0xdc,0x53,0x8d}
#define SMP_BLE_CHAR_UUID    {0x48,0x7c,0x99,0x74,0x11,0x26,0x9e,0xae,0x01,0x4e,0xce,0xfb,0x28,0x78,0x2e,0xda}

/* Reassembly of one SMP packet split across GATT writes (host-testable). */
struct smp_reassembly {
    uint8_t data[1024];
    size_t length, expected;
    uint32_t last_ms;
};
void smp_reassembly_reset(struct smp_reassembly *r);
/* Append one write. Returns 1 when a complete packet is in r->data (length
 * r->length; caller must reset after use), 0 if more is needed, -1 if the
 * write was dropped (malformed/oversize). Partial packets older than 2 s
 * are discarded before appending. */
int smp_reassembly_add(struct smp_reassembly *r, const uint8_t *p, size_t n, uint32_t now_ms);

#ifndef ET100_SMP_BLE_HOST_TEST
struct gap_event;
uint8_t smp_ble_add_service(void);
void smp_ble_gap(struct gap_event *event);
/* Send one SMP response packet as notifications of at most MTU-3 bytes each.
 * Blocks the caller (OTA worker) until every segment completes. */
int smp_ble_send(const uint8_t *packet, size_t length, uint32_t tag);
/* ota_worker_send_fn adapter; failures are counted, the client times out. */
void smp_ble_respond(const uint8_t *packet, size_t length, uint32_t tag);
#endif
#endif
