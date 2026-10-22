#ifndef ET100_EUP_META_SERVICE_H
#define ET100_EUP_META_SERVICE_H
/* BLE GATT side of the EUP AIoT Communication Service (always registered;
 * read-only, no authentication — same as eup_gateway). */
#include <stdint.h>

/* Service E9E00000-4F4C-4F47-4943-455550000001, RX E9E00001, TX E9E00002
 * (Bluetooth little-endian byte order; only byte 12 differs). */
#define EUP_META_UUID(n) {0x01,0x00,0x00,0x50,0x55,0x45,0x43,0x49,0x47,0x4f,0x4c,0x4f,n,0x00,0xe0,0xe9}

struct gap_event;
uint8_t et100_meta_add_service(void);
void et100_meta_gap(struct gap_event *event);
/* Send pending response parts; call periodically from a task (serial loop). */
void et100_meta_poll(void);
/* BLE identity address in display order (same source as the GCHK identity). */
void et100_identity_mac(uint8_t mac[6]);
#endif
