#ifndef ET100_GATT_CLIENT_H
#define ET100_GATT_CLIENT_H
#include <stddef.h>
#include <stdint.h>
struct gap_event;
void et100_gatt_init(int storage_ready);
void et100_gatt_add_profile(void);
void et100_gatt_gap(struct gap_event *);
int et100_gatt_uart(const char *, char *, const char *);
void et100_gatt_poll(void (*)(const char *, size_t, void *), void *);
unsigned et100_gatt_state(void);
unsigned et100_gatt_link_connected(void);
#endif
