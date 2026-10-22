#include "ota_manager.h"
#include "mbedtls/sha256.h"
#include <string.h>

static void lock(const struct ota_manager *m) { if (m->env->lock) m->env->lock(); }
static void unlock(const struct ota_manager *m) { if (m->env->unlock) m->env->unlock(); }

struct region { struct nor_port *nor; uint32_t base; };
static int region_read(void *context, uint32_t off, void *data, size_t n)
{
    struct region *r = context;
    if (off > 0x200000 || n > 0x200000 - off) return -1;
    return r->nor->read(r->base + off, data, n);
}
static int journal_hash(struct nor_port *nor, uint32_t off, uint32_t n, uint8_t digest[32], void *context)
{
    (void)context;
    struct region r = { nor, off };
    return image_hash(region_read, &r, n, digest);
}

void ota_manager_init(struct ota_manager *m, const struct ota_env *env)
{
    memset(m, 0, sizeof *m);
    m->env = env;
    m->active_source = env->source;
}

void ota_manager_set_source(struct ota_manager *m, uint32_t source)
{
    m->active_source = source;
}

/* Single update owner across transports: a mutating request from another
 * transport within OTA_OWNER_HOLD_MS of the owner's last change is refused. */
static int claim(struct ota_manager *m)
{
    uint32_t now = m->env->now_ms ? m->env->now_ms() : 0;
    if (m->owner && m->owner != m->active_source &&
        (uint32_t)(now - m->owner_ms) < OTA_OWNER_HOLD_MS)
        return SMP_EBUSY;
    m->owner = m->active_source;
    m->owner_ms = now;
    return 0;
}

static int ensure_ready(struct ota_manager *m)
{
    if (m->ready) return 0;
    m->session = false;
    if (m->env->nor_prepare && m->env->nor_prepare()) return SMP_EUNKNOWN;
    if (ota_journal_recover(&m->journal, m->env->nor, journal_hash, NULL)) return SMP_EUNKNOWN;
    m->ready = true;
    return 0;
}

static void drop(struct ota_manager *m)
{
    /* Journal requires recover after any I/O error; keep NOR prepared. */
    m->ready = false;
    m->session = false;
    m->verified_valid = false;
}

static uint32_t committed_bytes(const struct ota_progress *p)
{
    uint32_t chunks = (p->manifest.length + OTA_JOURNAL_CHUNK_SIZE - 1) / OTA_JOURNAL_CHUNK_SIZE, n = 0;
    while (n < chunks && (p->bitmap[n / 8] >> (n % 8) & 1)) n++;
    uint32_t bytes = n * OTA_JOURNAL_CHUNK_SIZE;
    return bytes > p->manifest.length ? p->manifest.length : bytes;
}

static bool same_manifest(const struct ota_progress *p, const struct ota_manifest *m)
{
    return p->manifest.session_id == m->session_id && p->manifest.length == m->length &&
           p->manifest.target == m->target && !memcmp(p->manifest.sha256, m->sha256, 32);
}

static int boot_state(const struct ota_manager *m, struct boot_record *state)
{
    return boot_state_read(m->env->internal, state) ? SMP_EUNKNOWN : 0;
}

static void start_session(struct ota_manager *m, const struct ota_manifest *mf, uint32_t at)
{
    m->session = true;
    m->session_id = mf->session_id;
    m->length = mf->length;
    memcpy(m->sha256, mf->sha256, 32);
    m->buffer_base = at;
    m->buffer_len = 0;
    m->verified_valid = false;
}

static int begin(struct ota_manager *m, const struct smp_upload *r)
{
    if (r->image != 0 || r->sha_len != 32 || r->len < 560 || r->len > ET100_APP_SLOT_SIZE || r->off != 0)
        return SMP_EINVAL;
    struct ota_manifest mf = { 0 };
    for (unsigned i = 0; i < 8; i++) mf.session_id |= (uint64_t)r->sha[i] << (8 * i);
    if (!mf.session_id) mf.session_id = 1;
    mf.target = OTA_ET100;
    mf.source = m->active_source;
    mf.length = r->len;
    memcpy(mf.sha256, r->sha, 32);
    struct ota_progress *p = &m->journal.progress;
    if ((p->state == OTA_DOWNLOADING || p->state == OTA_VERIFIED) && same_manifest(p, &mf)) {
        /* Same image: resume (explicit takeover if the source changed). */
        if (ota_journal_resume(&m->journal, &mf)) { drop(m); return SMP_EUNKNOWN; }
        start_session(m, &mf, p->state == OTA_VERIFIED ? mf.length : committed_bytes(p));
        return 0;
    }
    if (p->state == OTA_DOWNLOADING || p->state == OTA_VERIFIED || p->state == OTA_FAILED) {
        if (ota_journal_cancel(&m->journal, p->manifest.session_id)) { drop(m); return SMP_EUNKNOWN; }
    }
    if (ota_journal_begin(&m->journal, &mf)) { drop(m); return SMP_EUNKNOWN; }
    start_session(m, &mf, 0);
    return 0;
}

static int flush(struct ota_manager *m)
{
    uint32_t index = m->buffer_base / OTA_JOURNAL_CHUNK_SIZE;
    if (ota_journal_write_chunk(&m->journal, m->session_id, index, m->buffer, m->buffer_len)) {
        drop(m);
        return SMP_ECORRUPT;
    }
    m->buffer_base += (uint32_t)m->buffer_len;
    m->buffer_len = 0;
    return 0;
}

static int upload(void *context, const struct smp_upload *r, uint32_t *next)
{
    struct ota_manager *m = context;
    struct boot_record state;
    int rc = claim(m);
    if (!rc) rc = ensure_ready(m);
    if (!rc) rc = boot_state(m, &state);
    if (rc) return rc;
    if (state.kind != BOOT_CONFIRMED) return SMP_EBADSTATE;  /* trial/pending in progress */
    if (r->has_len && r->off == 0) {
        rc = begin(m, r);
        if (rc) return rc;
    } else if (!m->session) {
        return SMP_EBADSTATE;                                 /* client must restart at off 0 */
    }
    uint32_t expected = m->buffer_base + (uint32_t)m->buffer_len;
    if (r->off != expected) {                                 /* resync: tell client where to go */
        *next = expected;
        return 0;
    }
    if (r->data_len > m->length - expected) return SMP_EINVAL;
    const uint8_t *data = r->data;
    size_t left = r->data_len;
    while (left) {
        size_t space = OTA_JOURNAL_CHUNK_SIZE - m->buffer_len, n = left < space ? left : space;
        memcpy(m->buffer + m->buffer_len, data, n);
        m->buffer_len += n;
        data += n;
        left -= n;
        if (m->buffer_len == OTA_JOURNAL_CHUNK_SIZE ||
            m->buffer_base + m->buffer_len == m->length) {
            if ((rc = flush(m))) return rc;
        }
    }
    *next = m->buffer_base + (uint32_t)m->buffer_len;
    return 0;
}

/* Finish the journal (if complete) and verify the Candidate image once; cached. */
static int verify_candidate(struct ota_manager *m)
{
    if (m->verified_valid) return m->verified_rc;
    int rc = ensure_ready(m);
    if (rc) return rc;
    struct ota_progress *p = &m->journal.progress;
    if (p->state == OTA_DOWNLOADING && committed_bytes(p) == p->manifest.length) {
        if (ota_journal_finish(&m->journal, p->manifest.session_id) == OTA_EHASH) {
            m->verified_valid = true;
            return m->verified_rc = SMP_ECORRUPT;
        }
        if (p->state != OTA_VERIFIED) { drop(m); return SMP_EUNKNOWN; }
    }
    if (p->state != OTA_VERIFIED) return SMP_ENOENT;
    struct region r = { m->env->nor, OTA_CANDIDATE_BASE };
    m->verified_rc = image_candidate_verify(region_read, &r, p->manifest.length,
                                            p->manifest.sha256, &m->candidate) ? SMP_ECORRUPT : 0;
    m->verified_valid = true;
    return m->verified_rc;
}

static void version_text(const uint8_t header[32], char out[32])
{
    unsigned major = header[20], minor = header[21];
    unsigned revision = (unsigned)header[22] | (unsigned)header[23] << 8;
    unsigned long build = (unsigned long)header[24] | (unsigned long)header[25] << 8 |
                          (unsigned long)header[26] << 16 | (unsigned long)header[27] << 24;
    /* Zephyr formats "major.minor.revision.build". */
    char *p = out;
    unsigned long parts[4] = { major, minor, revision, build };
    for (unsigned i = 0; i < 4; i++) {
        char digits[11];
        unsigned n = 0;
        unsigned long v = parts[i];
        do { digits[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < sizeof digits);
        if (i) *p++ = '.';
        while (n) *p++ = digits[--n];
    }
    *p = 0;
}

static int running_image(const struct ota_manager *m, struct image_info *info)
{
    return mcuboot_image_verify(m->env->app_slot, ET100_APP_SLOT_SIZE, info) ? SMP_EUNKNOWN : 0;
}

static int image_state(void *context, struct smp_image_slot slots[2])
{
    struct ota_manager *m = context;
    struct boot_record state;
    struct image_info running;
    if (running_image(m, &running) || boot_state(m, &state)) return SMP_EUNKNOWN;
    slots[0].present = true;
    slots[0].slot = 0;
    version_text(m->env->app_slot, slots[0].version);
    memcpy(slots[0].hash, running.sha256, 32);
    slots[0].bootable = slots[0].active = true;
    /* The running image stays confirmed while a candidate is pending; only TRIAL is unconfirmed. */
    slots[0].confirmed = state.kind != BOOT_TRIAL;
    if (verify_candidate(m) == 0 && memcmp(m->candidate.sha256, running.sha256, 32)) {
        uint8_t header[32];
        if (!m->env->nor->read(OTA_CANDIDATE_BASE, header, sizeof header)) {
            slots[1].present = true;
            slots[1].slot = 1;
            version_text(header, slots[1].version);
            memcpy(slots[1].hash, m->candidate.sha256, 32);
            slots[1].bootable = true;
            slots[1].pending = state.kind == BOOT_PENDING &&
                               !memcmp(state.hash, m->journal.progress.manifest.sha256, 32);
        }
    }
    return 0;
}

static int image_test(void *context, const uint8_t hash[32])
{
    struct ota_manager *m = context;
    struct boot_record state;
    int rc = claim(m);
    if (!rc) rc = verify_candidate(m);
    if (rc) return rc;
    if (memcmp(hash, m->candidate.sha256, 32)) return SMP_ENOENT;
    if ((rc = boot_state(m, &state))) return rc;
    if (state.kind == BOOT_PENDING && !memcmp(state.hash, m->journal.progress.manifest.sha256, 32))
        return 0;                                    /* already pending: idempotent */
    if (state.kind != BOOT_CONFIRMED) return SMP_EBADSTATE;
    state.kind = BOOT_PENDING;
    state.length = m->journal.progress.manifest.length;
    state.reset_mask = 0;
    memcpy(state.hash, m->journal.progress.manifest.sha256, 32);
    lock(m);
    rc = boot_state_write(m->env->internal, &state);
    unlock(m);
    return rc ? SMP_EUNKNOWN : 0;
}

static int image_confirm(void *context, const uint8_t *hash)
{
    struct ota_manager *m = context;
    struct boot_record state;
    struct image_info running;
    int rc = claim(m);
    if (!rc) rc = running_image(m, &running);
    if (!rc) rc = boot_state(m, &state);
    if (rc) return rc;
    if (hash && memcmp(hash, running.sha256, 32)) return SMP_ENOTSUP;  /* no permanent install */
    if (state.kind == BOOT_CONFIRMED) return 0;
    if (state.kind != BOOT_TRIAL) return SMP_EBADSTATE;
    uint8_t full[32];
    if (mbedtls_sha256_ret(m->env->app_slot, running.size, full, 0)) return SMP_EUNKNOWN;
    lock(m);
    rc = boot_state_confirm(m->env->internal, full);
    unlock(m);
    return rc ? SMP_EBADSTATE : 0;
}

void ota_manager_smp_ops(struct ota_manager *m, struct smp_ops *ops)
{
    memset(ops, 0, sizeof *ops);
    ops->context = m;
    ops->image_state = image_state;
    ops->upload = upload;
    ops->image_test = image_test;
    ops->image_confirm = image_confirm;
    ops->buf_size = 1024;
}
