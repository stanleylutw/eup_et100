#ifndef ET100_SERIAL_GCHK_H
#define ET100_SERIAL_GCHK_H

#include <stddef.h>
#include <stdint.h>

#define ET100_SERIAL_LINE_MAX 128
#define ET100_GTIME_DEFAULT 15
#define ET100_GTIME_MAX 1800
typedef void (*et100_serial_send_fn)(const char *, size_t, void *);
typedef int (*et100_serial_save_fn)(uint16_t, void *);
typedef int (*et100_serial_save_cids_fn)(const uint16_t[4], void *);

struct et100_serial_parser {
    char line[ET100_SERIAL_LINE_MAX];
    size_t length;
    unsigned char discard;
    unsigned char cr;
    char identity[13];
    et100_serial_send_fn send;
    void *context;
    uint16_t gtime;
    unsigned char storage_ready;
    et100_serial_save_fn save;
    void *storage_context;
    uint16_t cids[4];
    unsigned cid_revision;
    et100_serial_save_cids_fn save_cids;
    /* OTABAUD: baud requested by the host, applied by the transport after the
     * response is transmitted (0 = none). Only 9600 and 115200 are accepted. */
    uint32_t baud_request;
    unsigned char baud_supported;
};

void et100_serial_init(struct et100_serial_parser *, const char *,
                       et100_serial_send_fn, void *);
void et100_serial_reset(struct et100_serial_parser *);
void et100_serial_feed(struct et100_serial_parser *, uint8_t);
void et100_serial_set_storage(struct et100_serial_parser *, int, uint16_t,
                             et100_serial_save_fn, void *);
void et100_serial_storage_ready(int);
void et100_serial_set_cid_storage(struct et100_serial_parser *, const uint16_t[4],
                                  et100_serial_save_cids_fn);

#endif
