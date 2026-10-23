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
    if (env->sensor) m->sensor_status = env->sensor->status;
}

/* Publish the store's status for other tasks (GSOTAPKG runs in the serial task). */
static void publish(struct ota_manager *m)
{
    lock(m);
    m->sensor_status = m->env->sensor->status;
    unlock(m);
}

bool ota_manager_sensor_supported(const struct ota_manager *m)
{
    return m->env->sensor != NULL;
}

void ota_manager_sensor_status(struct ota_manager *m, struct sensor_store_status *out)
{
    lock(m);
    *out = m->sensor_status;
    unlock(m);
}

void ota_manager_set_source(struct ota_manager *m, uint32_t source)
{
    m->active_source = source;
}

bool ota_manager_upload_open(const struct ota_manager *m) { return m->session; }

/* Single update owner across transports: a mutating request from another
 * transport within OTA_OWNER_HOLD_MS of the owner's last change is refused. */
static int claim(struct ota_manager *m)
{
    uint32_t now = m->env->now_ms ? m->env->now_ms() : 0;
    if (m->relay) return SMP_EBUSY;
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
    m->session_target = mf->target;
    m->session_source = mf->source;
    m->session_id = mf->session_id;
    m->length = mf->length;
    memcpy(m->sha256, mf->sha256, 32);
    m->buffer_base = at;
    m->buffer_len = 0;
    m->verified_valid = false;
}

static int sensor_finish(struct ota_manager *m);

static int begin(struct ota_manager *m, const struct smp_upload *r)
{
    bool sensor = r->image == 1;          /* v0.8.8: image 1 = Gateway sensor package */
    if (sensor && !m->env->sensor) return SMP_ENOTSUP;
    uint32_t min = sensor ? SENSOR_PKG_HEADER_SIZE + 32 : 560;
    uint32_t max = sensor ? SENSOR_PKG_MAX_SIZE : ET100_APP_SLOT_SIZE;
    if (r->image > 1 || r->sha_len != 32 || r->len < min || r->len > max || r->off != 0)
        return SMP_EINVAL;
    struct ota_manifest mf = { 0 };
    for (unsigned i = 0; i < 8; i++) mf.session_id |= (uint64_t)r->sha[i] << (8 * i);
    if (!mf.session_id) mf.session_id = 1;
    mf.target = sensor ? OTA_SENSOR : OTA_ET100;
    mf.source = m->active_source;
    mf.length = r->len;
    memcpy(mf.sha256, r->sha, 32);
    struct ota_progress *p = &m->journal.progress;
    if ((p->state == OTA_DOWNLOADING || p->state == OTA_VERIFIED) && same_manifest(p, &mf)) {
        /* Same image: resume (explicit takeover if the source changed). */
        if (ota_journal_resume(&m->journal, &mf)) { drop(m); return SMP_EUNKNOWN; }
        start_session(m, &mf, p->state == OTA_VERIFIED ? mf.length : committed_bytes(p));
        /* Complete package from before a reset (journal finished or not yet):
         * finish, validate and commit it now; the client is told off=len. */
        bool complete = p->state == OTA_VERIFIED || committed_bytes(p) == mf.length;
        return sensor && complete ? sensor_finish(m) : 0;
    }
    if (p->state == OTA_DOWNLOADING || p->state == OTA_VERIFIED || p->state == OTA_FAILED) {
        if (ota_journal_cancel(&m->journal, p->manifest.session_id)) { drop(m); return SMP_EUNKNOWN; }
    }
    if (sensor) {
        /* D4: INVALID is durable before the first package byte reaches the NOR. */
        int rc = sensor_store_begin(m->env->sensor);
        publish(m);
        if (rc) return SMP_EUNKNOWN;
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

/* MCUboot ih_ver (header bytes 20..22) compared like Zephyr img_mgmt_vercmp with
 * its default config: major, minor, revision; the build number is ignored. */
static int version_compare(const uint8_t *a, const uint8_t *b)
{
    uint32_t va[3] = { a[20], a[21], (uint32_t)a[22] | (uint32_t)a[23] << 8 };
    uint32_t vb[3] = { b[20], b[21], (uint32_t)b[22] | (uint32_t)b[23] << 8 };
    for (unsigned i = 0; i < 3; i++)
        if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
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
    if (r->upgrade && r->off == 0 && r->image == 0) {
        /* MCUmgr "upgrade": reject unless newer than the running image
         * (Zephyr: CURRENT_VERSION_IS_NEWER, legacy rc EBADSTATE). */
        if (r->data_len < 28) return SMP_EINVAL;
        if (version_compare(r->data, m->env->app_slot) <= 0) return SMP_EBADSTATE;
    }
    if (r->has_len && r->off == 0) {
        rc = begin(m, r);
        if (rc) return rc;
    } else if (!m->session || m->session_source != m->active_source) {
        /* Another transport must re-send the first packet (len/sha): begin()
         * then verifies the manifest and records an explicit takeover. */
        return SMP_EBADSTATE;
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
    if (m->session_target == OTA_SENSOR && *next == m->length && r->data_len)
        return sensor_finish(m);
    return 0;
}

/* Whole sensor package in the journal: finish (SHA-256 of the upload), validate
 * the Relay Header package from the NOR, then commit VALID with its header. */
static int sensor_finish(struct ota_manager *m)
{
    struct ota_progress *p = &m->journal.progress;
    struct sensor_store *store = m->env->sensor;
    if (p->state == OTA_DOWNLOADING) {
        int rc = ota_journal_finish(&m->journal, p->manifest.session_id);
        if (rc == OTA_EHASH) { m->session = false; return SMP_ECORRUPT; }
        if (rc || p->state != OTA_VERIFIED) { drop(m); return SMP_EUNKNOWN; }
    }
    if (p->state != OTA_VERIFIED || p->manifest.target != OTA_SENSOR) return SMP_EUNKNOWN;
    if (store->status.state == SENSOR_STORE_VALID) return 0;    /* this session, committed */
    struct region r = { m->env->nor, SENSOR_STORE_NOR_BASE };
    struct sensor_pkg_info info;
    uint8_t header[SENSOR_PKG_HEADER_SIZE];
    if (sensor_pkg_validate(region_read, &r, p->manifest.length, &info)) {
        m->session = false;
        return SMP_ECORRUPT;                                    /* record stays INVALID */
    }
    int rc = region_read(&r, 0, header, sizeof header) ||
             sensor_store_commit(store, header, p->manifest.length);
    publish(m);
    if (rc) return SMP_EUNKNOWN;
    m->session = false;
    return 0;
}

int ota_manager_sensor_delete(struct ota_manager *m)
{
    struct sensor_store *store = m->env->sensor;
    if (!store) return SMP_ENOTSUP;
    int rc = claim(m);
    if (!rc) rc = ensure_ready(m);
    if (rc) return rc;
    struct ota_progress *p = &m->journal.progress;
    /* A finished sensor session must not be resumed onto erased bytes. */
    if (p->manifest.target == OTA_SENSOR &&
        (p->state == OTA_DOWNLOADING || p->state == OTA_VERIFIED || p->state == OTA_FAILED) &&
        ota_journal_cancel(&m->journal, p->manifest.session_id)) {
        drop(m);
        return SMP_EUNKNOWN;
    }
    if (m->session_target == OTA_SENSOR) m->session = false;
    rc = sensor_store_mark_invalid(store);
    publish(m);
    if (rc) return SMP_EUNKNOWN;
    for (uint32_t off = 0; off < SENSOR_STORE_NOR_SIZE; off += 4096) {   /* ~13 ms each */
        if (m->env->nor->erase(SENSOR_STORE_NOR_BASE + off, 4096)) {
            drop(m);
            return SMP_EUNKNOWN;
        }
    }
    rc = sensor_store_remove(store);
    publish(m);
    return rc ? SMP_EUNKNOWN : 0;
}

void ota_manager_set_relay(struct ota_manager *m, bool active)
{
    m->relay = active;
}

int ota_manager_sensor_read(struct ota_manager *m, uint32_t offset, void *data, size_t length)
{
    struct sensor_store *store = m->env->sensor;
    if (!store || store->status.state != SENSOR_STORE_VALID ||
        offset > store->status.info.package_length ||
        length > store->status.info.package_length - offset)
        return SMP_EINVAL;
    int rc = ensure_ready(m);
    if (rc) return rc;
    struct region r = { m->env->nor, SENSOR_STORE_NOR_BASE };
    return region_read(&r, offset, data, length) ? SMP_EUNKNOWN : 0;
}

/* Finish the journal (if complete) and verify the Candidate image once; cached. */
static int verify_candidate(struct ota_manager *m)
{
    if (m->verified_valid) return m->verified_rc;
    int rc = ensure_ready(m);
    if (rc) return rc;
    struct ota_progress *p = &m->journal.progress;
    if (p->manifest.target != OTA_ET100) return SMP_ENOENT;   /* sensor package is not slot 1 */
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
            slots[1].permanent = slots[1].pending && m->pending_confirm;
        }
    }
    return 0;
}

static int mark_pending(struct ota_manager *m, const uint8_t hash[32])
{
    struct boot_record state;
    int rc = verify_candidate(m);
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

static int image_test(void *context, const uint8_t hash[32])
{
    struct ota_manager *m = context;
    int rc = claim(m);
    if (!rc) rc = mark_pending(m, hash);
    if (!rc) m->pending_confirm = false;
    return rc;
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
    if (hash && memcmp(hash, running.sha256, 32)) {
        /* "Confirm only" clients (nRF libraries' default) confirm the uploaded image
         * directly. There is no permanent install: it becomes PENDING exactly like
         * "test", and the new App confirms itself after its health check, so the
         * installer backup/rollback still applies (06 §5). */
        rc = mark_pending(m, hash);
        if (!rc) m->pending_confirm = true;
        return rc;
    }
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
