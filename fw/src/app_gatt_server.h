#ifndef ET100_APP_GATT_SERVER_H
#define ET100_APP_GATT_SERVER_H
#include "app_manager.h"

#define ET100_APP_NAME "EUP_AIoT_HUB_FR"
/* Bluetooth little-endian encoding of 8D2F000n-9F8A-4F6F-9A7B-7D5F4F455550. */
#define ET100_APP_UUID(n) {0x50,0x55,0x45,0x4f,0x5f,0x7d,0x7b,0x9a,0x6f,0x4f,0x8a,0x9f,n,0,0x2f,0x8d}
struct gap_event;
uint8_t et100_app_server_add(void);
void et100_app_server_gap(struct gap_event *);
void et100_app_server_poll(void);
unsigned et100_app_server_active(void);
void et100_app_server_clear(void);
void et100_app_server_receive(const uint8_t[6], uint8_t, int8_t, uint8_t,
    const uint8_t *, size_t, const struct et100_adv_details *, uint32_t);
#endif
