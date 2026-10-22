#ifndef ET100_OTA_MANAGER_H
#define ET100_OTA_MANAGER_H
/* Single-session OTA manager inside the App (06 §8.1 step 2). Owns the download
 * journal and NOR Candidate; maps SMP image upload/state/test/confirm onto the
 * journal and the shared boot state. One caller task at a time (v0.8.5: the
 * serial task); BLE (v0.8.6) must serialize through the same owner. */
#include "boot_state.h"
#include "mcuboot_image.h"
#include "ota_journal.h"
#include "smp_server.h"
#include "sensor_store.h"

#define OTA_CANDIDATE_BASE 0x020000u

struct ota_env {
    struct nor_port *nor;
    int (*nor_prepare)(void);             /* init + authorize writes; may be slow */
    const struct internal_port *internal; /* boot state region only */
    const uint8_t *app_slot;              /* memory-mapped App slot (0x08020000) */
    void (*lock)(void);                   /* serialize internal-flash writes */
    void (*unlock)(void);
    uint32_t source;                      /* default enum ota_source */
    uint32_t (*now_ms)(void);             /* for cross-transport ownership; may be NULL */
    struct sensor_store *sensor;          /* v0.8.8 SMP image 1; NULL = unsupported */
};
/* A transport that changed state holds the session for this long; others get EBUSY. */
#define OTA_OWNER_HOLD_MS 30000u
struct ota_manager {
    const struct ota_env *env;
    struct ota_journal journal;
    bool ready, session, verified_valid;
    bool pending_confirm;          /* candidate made PENDING by "confirm" (reported permanent) */
    int verified_rc;
    uint64_t session_id;
    uint32_t length, buffer_base;
    size_t buffer_len;
    uint8_t sha256[32];
    struct image_info candidate;
    uint32_t active_source, owner, owner_ms, session_source, session_target;
    struct sensor_store_status sensor_status;   /* published copy for other tasks */
    uint8_t buffer[OTA_JOURNAL_CHUNK_SIZE];
};
void ota_manager_init(struct ota_manager *m, const struct ota_env *env);
/* Transport of the request about to be processed (enum ota_source). */
void ota_manager_set_source(struct ota_manager *m, uint32_t source);
/* Fill smp_ops with this manager (context = m). */
void ota_manager_smp_ops(struct ota_manager *m, struct smp_ops *ops);
/* v0.8.8 sensor package: snapshot is safe from any task; delete runs in the
 * manager's task (marks INVALID, erases the NOR slot, removes the record).
 * Returns 0, SMP_EBUSY (another transport owns the update) or SMP_EUNKNOWN. */
bool ota_manager_sensor_supported(const struct ota_manager *m);
void ota_manager_sensor_status(struct ota_manager *m, struct sensor_store_status *out);
int ota_manager_sensor_delete(struct ota_manager *m);
#endif
