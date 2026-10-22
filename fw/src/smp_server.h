#ifndef ET100_SMP_SERVER_H
#define ET100_SMP_SERVER_H
/* Transport-independent SMP (MCUmgr) server subset shared by serial (v0.8.5) and BLE
 * (v0.8.6). Groups: OS(0) echo(0)/reset(5)/mcumgr-params(6)/bootloader-info(8),
 * IMG(1) state(0)/upload(1). */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SMP_HEADER_SIZE 8u
enum smp_op { SMP_OP_READ = 0, SMP_OP_READ_RSP = 1, SMP_OP_WRITE = 2, SMP_OP_WRITE_RSP = 3 };
enum smp_group { SMP_GROUP_OS = 0, SMP_GROUP_IMG = 1 };
enum smp_err { SMP_EOK = 0, SMP_EUNKNOWN = 1, SMP_ENOMEM = 2, SMP_EINVAL = 3,
               SMP_ETIMEOUT = 4, SMP_ENOENT = 5, SMP_EBADSTATE = 6, SMP_EMSGSIZE = 7,
               SMP_ENOTSUP = 8, SMP_ECORRUPT = 9, SMP_EBUSY = 10 };

struct smp_image_slot {
    bool present;
    uint32_t slot;
    char version[24];
    uint8_t hash[32];          /* MCUboot image hash (SHA-256 TLV) */
    bool bootable, pending, confirmed, active, permanent;
};
struct smp_upload {
    uint32_t image, off, len;
    bool has_len;
    const uint8_t *data;
    size_t data_len;
    const uint8_t *sha;        /* full-file SHA-256, first packet only */
    size_t sha_len;
};
struct smp_ops {
    void *context;
    int (*image_state)(void *context, struct smp_image_slot slots[2]);
    /* Returns smp_err; *next_off is the next offset the server expects. */
    int (*upload)(void *context, const struct smp_upload *request, uint32_t *next_off);
    int (*image_test)(void *context, const uint8_t hash[32]);
    int (*image_confirm)(void *context, const uint8_t *hash /* NULL = running image */);
    uint32_t buf_size;             /* reported by OS mcumgr-params */
};
/* Process one request packet; writes the response packet into rsp.
 * Returns response length (0 = no response, e.g. malformed header). */
size_t smp_server_process(const struct smp_ops *ops, const uint8_t *request, size_t length,
                          uint8_t *rsp, size_t capacity, bool *reset_requested);
#endif
