#include "smp_server.h"
#include "cbor_min.h"
#include <string.h>

struct header {
    uint8_t op, version, flags, sequence, id;
    uint16_t length, group;
};

static bool parse_header(const uint8_t *p, size_t n, struct header *h)
{
    if (n < SMP_HEADER_SIZE) return false;
    h->op = p[0] & 7;
    h->version = (p[0] >> 3) & 3;
    h->flags = p[1];
    h->length = (uint16_t)(p[2] << 8 | p[3]);
    h->group = (uint16_t)(p[4] << 8 | p[5]);
    h->sequence = p[6];
    h->id = p[7];
    return h->length == n - SMP_HEADER_SIZE &&
           (h->op == SMP_OP_READ || h->op == SMP_OP_WRITE);
}

static size_t finish(const struct header *h, uint8_t *rsp, const struct cbor_writer *w)
{
    if (w->error) return 0;
    rsp[0] = (uint8_t)((h->version & 3) << 3 | (h->op + 1));
    rsp[1] = 0;
    rsp[2] = (uint8_t)(w->len >> 8);
    rsp[3] = (uint8_t)w->len;
    rsp[4] = (uint8_t)(h->group >> 8);
    rsp[5] = (uint8_t)h->group;
    rsp[6] = h->sequence;
    rsp[7] = h->id;
    return SMP_HEADER_SIZE + w->len;
}

static void put_rc(struct cbor_writer *w, int rc)
{
    cbor_put_map(w, 1);
    cbor_put_text(w, "rc");
    cbor_put_uint(w, (uint64_t)rc);
}

static void put_states(struct cbor_writer *w, const struct smp_ops *ops)
{
    struct smp_image_slot slots[2];
    memset(slots, 0, sizeof slots);
    if (ops->image_state(ops->context, slots)) {
        put_rc(w, SMP_EUNKNOWN);
        return;
    }
    size_t count = (size_t)slots[0].present + (size_t)slots[1].present;
    cbor_put_map(w, 2);
    cbor_put_text(w, "images");
    cbor_put_array(w, count);
    for (unsigned i = 0; i < 2; i++) {
        const struct smp_image_slot *s = &slots[i];
        if (!s->present) continue;
        cbor_put_map(w, 9);
        cbor_put_text(w, "image");     cbor_put_uint(w, 0);
        cbor_put_text(w, "slot");      cbor_put_uint(w, s->slot);
        cbor_put_text(w, "version");   cbor_put_text(w, s->version);
        cbor_put_text(w, "hash");      cbor_put_bytes(w, s->hash, 32);
        cbor_put_text(w, "bootable");  cbor_put_bool(w, s->bootable);
        cbor_put_text(w, "pending");   cbor_put_bool(w, s->pending);
        cbor_put_text(w, "confirmed"); cbor_put_bool(w, s->confirmed);
        cbor_put_text(w, "active");    cbor_put_bool(w, s->active);
        cbor_put_text(w, "permanent"); cbor_put_bool(w, s->permanent);
    }
    cbor_put_text(w, "splitStatus");
    cbor_put_uint(w, 0);
}

static bool get_uint(const uint8_t *m, size_t n, const char *key, uint32_t *out, bool *present)
{
    struct cbor_value v;
    int rc = cbor_map_find(m, n, key, &v);
    *present = rc == 0;
    if (rc) return rc == 1;
    if (v.type != CBOR_UINT || v.u > UINT32_MAX) return false;
    *out = (uint32_t)v.u;
    return true;
}

static void handle_upload(const struct smp_ops *ops, const uint8_t *m, size_t n,
                          struct cbor_writer *w)
{
    struct smp_upload r;
    struct cbor_value v;
    bool present;
    memset(&r, 0, sizeof r);
    if (!get_uint(m, n, "off", &r.off, &present) || !present ||
        cbor_map_find(m, n, "data", &v) || v.type != CBOR_BYTES ||
        !get_uint(m, n, "image", &r.image, &present) ||
        !get_uint(m, n, "len", &r.len, &r.has_len)) {
        put_rc(w, SMP_EINVAL);
        return;
    }
    r.data = v.ptr;
    r.data_len = v.len;
    int rc = cbor_map_find(m, n, "sha", &v);
    if (rc < 0 || (rc == 0 && v.type != CBOR_BYTES)) {
        put_rc(w, SMP_EINVAL);
        return;
    }
    if (rc == 0) {
        r.sha = v.ptr;
        r.sha_len = v.len;
    }
    rc = cbor_map_find(m, n, "upgrade", &v);
    if (rc < 0 || (rc == 0 && v.type != CBOR_BOOL)) {
        put_rc(w, SMP_EINVAL);
        return;
    }
    r.upgrade = rc == 0 && v.u;
    uint32_t next = 0;
    rc = ops->upload(ops->context, &r, &next);
    if (rc) {
        put_rc(w, rc);
        return;
    }
    cbor_put_map(w, 2);
    cbor_put_text(w, "rc");
    cbor_put_uint(w, 0);
    cbor_put_text(w, "off");
    cbor_put_uint(w, next);
}

static void handle_state_write(const struct smp_ops *ops, const uint8_t *m, size_t n,
                               struct cbor_writer *w)
{
    struct cbor_value hash, confirm;
    int has_hash = cbor_map_find(m, n, "hash", &hash);
    int has_confirm = cbor_map_find(m, n, "confirm", &confirm);
    if (has_hash < 0 || has_confirm < 0 ||
        (has_hash == 0 && (hash.type != CBOR_BYTES || hash.len != 32)) ||
        (has_confirm == 0 && confirm.type != CBOR_BOOL)) {
        put_rc(w, SMP_EINVAL);
        return;
    }
    bool want_confirm = has_confirm == 0 && confirm.u;
    int rc;
    if (want_confirm) rc = ops->image_confirm(ops->context, has_hash == 0 ? hash.ptr : NULL);
    else if (has_hash == 0) rc = ops->image_test(ops->context, hash.ptr);
    else rc = SMP_EINVAL;
    if (rc) put_rc(w, rc);
    else put_states(w, ops);
}

size_t smp_server_process(const struct smp_ops *ops, const uint8_t *request, size_t length,
                          uint8_t *rsp, size_t capacity, bool *reset_requested)
{
    struct header h;
    struct cbor_writer w;
    *reset_requested = false;
    if (capacity < SMP_HEADER_SIZE || !parse_header(request, length, &h)) return 0;
    const uint8_t *m = request + SMP_HEADER_SIZE;
    size_t n = h.length;
    cbor_writer_init(&w, rsp + SMP_HEADER_SIZE, capacity - SMP_HEADER_SIZE);
    if (!cbor_map_valid(m, n)) {
        put_rc(&w, SMP_EINVAL);
        return finish(&h, rsp, &w);
    }
    bool write = h.op == SMP_OP_WRITE;
    if (h.group == SMP_GROUP_OS && h.id == 0 && write) {           /* echo */
        struct cbor_value d;
        if (cbor_map_find(m, n, "d", &d) || d.type != CBOR_TEXT || d.len > 128) {
            put_rc(&w, SMP_EINVAL);
        } else {
            char text[129];
            memcpy(text, d.ptr, d.len);
            text[d.len] = 0;
            cbor_put_map(&w, 1);
            cbor_put_text(&w, "r");
            cbor_put_text(&w, text);
        }
    } else if (h.group == SMP_GROUP_OS && h.id == 5 && write) {    /* reset (Zephyr OS_MGMT_ID_RESET) */
        cbor_put_map(&w, 0);
        *reset_requested = true;
    } else if (h.group == SMP_GROUP_OS && h.id == 6 && !write) {   /* mcumgr params */
        cbor_put_map(&w, 2);
        cbor_put_text(&w, "buf_size");
        cbor_put_uint(&w, ops->buf_size);
        cbor_put_text(&w, "buf_count");
        cbor_put_uint(&w, 1);
    } else if (h.group == SMP_GROUP_OS && h.id == 8 && !write) {   /* bootloader info */
        /* nRF Connect Device Manager picks its upgrade flow from this. Mode 3
         * ("swap without scratch") = test/confirm with revert, which matches
         * installer backup + trial + rollback (06 §5). */
        struct cbor_value q;
        int has_query = cbor_map_find(m, n, "query", &q);
        if (has_query == 1) {
            cbor_put_map(&w, 1);
            cbor_put_text(&w, "bootloader");
            cbor_put_text(&w, "MCUboot");
        } else if (q.type == CBOR_TEXT && q.len == 4 && !memcmp(q.ptr, "mode", 4)) {
            cbor_put_map(&w, 2);
            cbor_put_text(&w, "mode");
            cbor_put_uint(&w, 3);
            cbor_put_text(&w, "no-downgrade");
            cbor_put_bool(&w, false);
        } else {
            put_rc(&w, SMP_ENOTSUP);
        }
    } else if (h.group == SMP_GROUP_IMG && h.id == 0) {            /* image state */
        if (write) handle_state_write(ops, m, n, &w);
        else put_states(&w, ops);
    } else if (h.group == SMP_GROUP_IMG && h.id == 1 && write) {   /* upload */
        handle_upload(ops, m, n, &w);
    } else {
        put_rc(&w, SMP_ENOTSUP);
    }
    return finish(&h, rsp, &w);
}
