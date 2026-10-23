#ifndef ET100_SERIAL_GCHK_H
#define ET100_SERIAL_GCHK_H

#include <stddef.h>
#include <stdint.h>
#include "sensor_store.h"
#include "inst_status.h"

struct eup_fw_info;

#define ET100_SERIAL_LINE_MAX 128
#define ET100_GTIME_DEFAULT 15
#define ET100_GTIME_MAX 1800
typedef void (*et100_serial_send_fn)(const char *, size_t, void *);
typedef int (*et100_serial_save_fn)(uint16_t, void *);
typedef int (*et100_serial_save_cids_fn)(const uint16_t[4], void *);
#define ET100_REPORT_CONTROLS_SIZE 102
struct et100_report_controls {
    uint8_t macs[8][6]; /* Display order, unlike the BLE SDK address bytes. */
    uint8_t maxrpt;
};
typedef int (*et100_serial_save_controls_fn)(const struct et100_report_controls *, void *);

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
    struct et100_report_controls controls;
    et100_serial_save_controls_fn save_controls;
    /* OTABAUD: baud requested by the host, applied by the transport after the
     * response is transmitted (0 = none). Only 9600 and 115200 are accepted. */
    uint32_t baud_request;
    unsigned char baud_supported;
    /* GFWINFO source (App EUPFWV1 block); NULL answers ";0;". */
    const struct eup_fw_info *fw_info;
    int (*ota_status)(struct inst_snapshot *);
    /* v0.8.8 sensor package (format of eup_gateway GSOTAPKG/DSOTAPKG). pkg_status
     * NULL answers UNSUPPORTED. DSOTAPKG sets pkg_delete_request; the transport
     * queues the erase and answers with et100_serial_pkg_deleted(). */
    int (*pkg_status)(struct sensor_store_status *);
    unsigned char pkg_delete_request, pkg_delete_busy;
    char pkg_delete_token[5];
};

void et100_serial_init(struct et100_serial_parser *, const char *,
                       et100_serial_send_fn, void *);
void et100_serial_reset(struct et100_serial_parser *);
void et100_serial_feed(struct et100_serial_parser *, uint8_t);
void et100_serial_set_storage(struct et100_serial_parser *, int, uint16_t,
                             et100_serial_save_fn, void *);
void et100_serial_storage_ready(int);
/* rc: 0 OK, SMP_EBUSY, SMP_ENOTSUP, other = DELETE_FAILED. */
void et100_serial_pkg_deleted(struct et100_serial_parser *, int rc);
void et100_serial_set_cid_storage(struct et100_serial_parser *, const uint16_t[4],
                                  et100_serial_save_cids_fn);
void et100_controls_encode(const struct et100_report_controls *, uint8_t[ET100_REPORT_CONTROLS_SIZE]);
int et100_controls_decode(struct et100_report_controls *, const uint8_t[ET100_REPORT_CONTROLS_SIZE], uint16_t);

#endif
