#ifndef ET100_SENSOR_SMP_CLIENT_H
#define ET100_SENSOR_SMP_CLIENT_H
/* v0.8.9: MCUmgr SMP client towards a Sensor, ported from eup_gateway
 * src/ota/sensor_smp_client.c (same requests, CBOR keys, checks and limits).
 * The BLE link is abstracted so the client runs on host tests unchanged.
 * Errors are negative errno values, as in the gateway. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SENSOR_SMP_CHUNK_MAX 768u
#define SENSOR_SMP_COMMAND_TIMEOUT_MS 5000u
#define SENSOR_SMP_ERASE_TIMEOUT_MS 15000u
#define SENSOR_SMP_UPLOAD_DEADLINE_MS 120000u

struct sensor_smp_link {
    /* One ATT write-without-response payload (<= mtu - 3); 0 = queued. */
    int (*write)(const uint8_t *data, size_t length);
    uint16_t (*mtu)(void);
    /* Wait for *flag: 0, -ETIMEDOUT, -ENOTCONN or -ECANCELED. */
    int (*wait)(volatile unsigned *flag, uint32_t timeout_ms);
    /* Sensor image bytes; offset 0 is the first byte after the 256 B package header. */
    int (*read_image)(uint32_t offset, void *data, size_t length);
    uint32_t (*now_ms)(void);
};

struct sensor_smp_image_state {
    uint8_t hash[32];
    uint32_t slot;
    bool active, pending, confirmed;
};

typedef void (*sensor_smp_progress_fn)(uint32_t bytes_done, void *ctx);
typedef void (*sensor_smp_erase_fn)(int result, uint32_t elapsed_ms, void *ctx);

/* Bind the link for this connection (after SMP discovery + notify enable). */
void sensor_smp_client_begin(const struct sensor_smp_link *link);
void sensor_smp_client_end(void);
/* Notification fragment from the SMP characteristic (BLE callback context). */
void sensor_smp_client_rx(const uint8_t *data, size_t length);

/* Upload the image: optional slot-1 erase first; allow_resume accepts a first
 * response offset beyond the chunk (target already holds the prefix). */
int sensor_smp_client_upload(uint32_t image_length, const uint8_t sha256[32], size_t chunk,
                             bool allow_resume, bool erase_first, uint32_t *bytes_done,
                             sensor_smp_progress_fn progress, sensor_smp_erase_fn erase_cb, void *ctx);
int sensor_smp_client_image_state(struct sensor_smp_image_state *images, size_t capacity, size_t *count);
/* Mark hash pending (confirm=false); 0 only if the target reports it pending. */
int sensor_smp_client_image_test(const uint8_t hash[32]);
/* OS reset; a link drop before the response counts as success. */
int sensor_smp_client_os_reset(void);
#endif
