#ifndef ET100_OTA_JOURNAL_H
#define ET100_OTA_JOURNAL_H
#include "nor_port.h"

#define OTA_JOURNAL_CHUNK_SIZE 4096
#define OTA_JOURNAL_ET100_MAX 2097152
#define OTA_JOURNAL_SENSOR_MAX 225280
#define OTA_JOURNAL_BITMAP_BYTES 64

enum ota_target { OTA_ET100 = 1, OTA_SENSOR = 2 };
enum ota_source { OTA_LTE = 1, OTA_BLE = 2, OTA_RS232 = 3 };
enum ota_state { OTA_NONE, OTA_DOWNLOADING, OTA_VERIFIED, OTA_FAILED, OTA_CANCELLED };
enum ota_error { OTA_ERECOVER = -20, OTA_EBUSY = -21, OTA_ESTATE = -22,
                 OTA_ECONTENT = -23, OTA_EINCOMPLETE = -24, OTA_EHASH = -25,
                 OTA_ESESSION = -26 };

struct ota_manifest {
    uint64_t session_id;
    uint32_t target, source, length;
    uint8_t sha256[32];
};
struct ota_progress {
    struct ota_manifest manifest;
    enum ota_state state;
    uint8_t bitmap[OTA_JOURNAL_BITMAP_BYTES];
};
/* Hash exactly length bytes through nor->read; return errors, never unbounded wait. */
typedef int (*ota_hash_fn)(struct nor_port *nor, uint32_t offset, uint32_t length,
                           uint8_t digest[32], void *context);
struct ota_journal {
    struct nor_port *nor;
    ota_hash_fn hash;
    void *hash_context;
    struct ota_progress progress;
    uint64_t sequence;
    int sector;
    unsigned next_slot;
    bool ready, fresh;
};

/* Must call recover after each reset or I/O error. No API initializes/arms NOR. */
int ota_journal_recover(struct ota_journal *j, struct nor_port *nor,
                        ota_hash_fn hash, void *context);
int ota_journal_begin(struct ota_journal *j, const struct ota_manifest *manifest);
int ota_journal_resume(struct ota_journal *j, const struct ota_manifest *manifest);
int ota_journal_write_chunk(struct ota_journal *j, uint64_t session_id, uint32_t index,
                            const void *data, size_t length);
int ota_journal_query(const struct ota_journal *j, struct ota_progress *out);
int ota_journal_finish(struct ota_journal *j, uint64_t session_id);
int ota_journal_cancel(struct ota_journal *j, uint64_t session_id);
#endif
