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

#define OTA_CANDIDATE_BASE 0x020000u

struct ota_env {
    struct nor_port *nor;
    int (*nor_prepare)(void);             /* init + authorize writes; may be slow */
    const struct internal_port *internal; /* boot state region only */
    const uint8_t *app_slot;              /* memory-mapped App slot (0x08020000) */
    void (*lock)(void);                   /* serialize internal-flash writes */
    void (*unlock)(void);
    uint32_t source;                      /* enum ota_source of this transport */
};
struct ota_manager {
    const struct ota_env *env;
    struct ota_journal journal;
    bool ready, session, verified_valid;
    int verified_rc;
    uint64_t session_id;
    uint32_t length, buffer_base;
    size_t buffer_len;
    uint8_t sha256[32];
    struct image_info candidate;
    uint8_t buffer[OTA_JOURNAL_CHUNK_SIZE];
};
void ota_manager_init(struct ota_manager *m, const struct ota_env *env);
/* Fill smp_ops with this manager (context = m). */
void ota_manager_smp_ops(struct ota_manager *m, struct smp_ops *ops);
#endif
