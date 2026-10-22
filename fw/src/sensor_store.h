#ifndef ET100_SENSOR_STORE_H
#define ET100_SENSOR_STORE_H
/* v0.8.8: persistent sensor package record (gateway NVS ID 13 equivalent) in the
 * internal Settings KVDB. EMPTY / INVALID / VALID + generation, same semantics as
 * eup_gateway src/ota/sensor_ota_store.c: INVALID is committed before the first
 * package byte is written, VALID only after full validation; delete marks INVALID,
 * erases, then removes the record (EMPTY). The validated 256 B header is kept in
 * the record so queries never touch the NOR (which the OTA worker owns). */
#include "sensor_package.h"
#include <stdbool.h>

#define SENSOR_STORE_NOR_BASE 0x420000u
#define SENSOR_STORE_NOR_SIZE 0x40000u           /* 256 KB slot, package <= 225280 B */
#define SENSOR_STORE_RECORD_SIZE 272u

enum sensor_store_state { SENSOR_STORE_EMPTY, SENSOR_STORE_INVALID, SENSOR_STORE_VALID };

struct sensor_store_ops {
    /* Returns record length (0 = absent); load fills at most capacity bytes. */
    size_t (*load)(uint8_t *record, size_t capacity);
    int (*save)(const uint8_t *record, size_t length);    /* 0 = written and read back */
    int (*remove)(void);                                  /* 0 = absent afterwards */
};
struct sensor_store_status {
    enum sensor_store_state state;
    uint32_t generation;
    struct sensor_pkg_info info;                          /* VALID only */
};
struct sensor_store {
    const struct sensor_store_ops *ops;
    struct sensor_store_status status;
};

void sensor_store_init(struct sensor_store *s, const struct sensor_store_ops *ops);
/* A new package upload starts: generation+1, INVALID. */
int sensor_store_begin(struct sensor_store *s);
/* Package fully written and validated: VALID with its header. */
int sensor_store_commit(struct sensor_store *s, const uint8_t header[SENSOR_PKG_HEADER_SIZE],
                        uint32_t package_length);
/* Delete: mark INVALID before erasing; then remove the record (EMPTY). */
int sensor_store_mark_invalid(struct sensor_store *s);
int sensor_store_remove(struct sensor_store *s);
#endif
