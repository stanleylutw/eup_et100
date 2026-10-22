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

/* v0.8.9 Sensor OTA relay (gateway eup_aiot_gatt_client exclusive mode). The
 * relay runs inside the GATT worker through the idle hook, so every call below
 * except reserve/unreserve/cancel/metadata is worker-context only. */
struct eup_gatt_metadata;
enum et100_gatt_stage {
    ET100_GATT_CONNECTED, ET100_GATT_AUTHENTICATING, ET100_GATT_AUTHENTICATED,
    ET100_GATT_METADATA_READING, ET100_GATT_METADATA_READY,
};
struct et100_gatt_exclusive_ops {
    uint32_t wait_ms;                                   /* exact-MAC advertising wait */
    void (*matched)(void *ctx);
    void (*stage)(enum et100_gatt_stage stage, int result, void *ctx);
    int (*run)(void *ctx);                              /* link up, AES + metadata done */
    void *ctx;
};
/* Hook called by the idle worker; returns nonzero if it did work. */
void et100_gatt_set_idle_hook(int (*hook)(void));
/* Reserve the single owner: auto/manual sessions and STYPESWITCH are refused. */
int et100_gatt_reserve(void);
void et100_gatt_unreserve(void);
void et100_gatt_cancel(void);
/* One session to mac; 0 or negative errno (ETIMEDOUT no adv, ENOTCONN, EACCES, ECANCELED, run()). */
int et100_gatt_exclusive(const uint8_t mac[6], const struct et100_gatt_exclusive_ops *ops);
int et100_gatt_metadata(const uint8_t mac[6], struct eup_gatt_metadata *out);
int et100_gatt_wait(volatile unsigned *flag, uint32_t timeout_ms);
uint16_t et100_gatt_mtu(void);
/* Discover the Sensor SMP service/characteristic, enable notify, route to sink. */
int et100_gatt_smp_open(void (*sink)(const uint8_t *, size_t));
int et100_gatt_smp_write(const uint8_t *data, size_t length);
void et100_gatt_op_event(const uint8_t mac[6], const char *data);
#endif
