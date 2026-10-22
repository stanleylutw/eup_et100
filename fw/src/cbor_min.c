#include "cbor_min.h"
#include <string.h>

void cbor_writer_init(struct cbor_writer *w, uint8_t *buf, size_t cap)
{
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
    w->error = false;
}

static void put_raw(struct cbor_writer *w, const void *data, size_t length)
{
    if (w->error || length > w->cap - w->len) {
        w->error = true;
        return;
    }
    memcpy(w->buf + w->len, data, length);
    w->len += length;
}

static void put_head(struct cbor_writer *w, uint8_t major, uint64_t value)
{
    uint8_t head[9];
    size_t n;
    if (value < 24) {
        head[0] = (uint8_t)(major << 5 | value);
        n = 1;
    } else if (value <= 0xff) {
        head[0] = (uint8_t)(major << 5 | 24);
        head[1] = (uint8_t)value;
        n = 2;
    } else if (value <= 0xffff) {
        head[0] = (uint8_t)(major << 5 | 25);
        head[1] = (uint8_t)(value >> 8);
        head[2] = (uint8_t)value;
        n = 3;
    } else if (value <= 0xffffffffu) {
        head[0] = (uint8_t)(major << 5 | 26);
        for (unsigned i = 0; i < 4; i++) head[1 + i] = (uint8_t)(value >> (24 - 8 * i));
        n = 5;
    } else {
        head[0] = (uint8_t)(major << 5 | 27);
        for (unsigned i = 0; i < 8; i++) head[1 + i] = (uint8_t)(value >> (56 - 8 * i));
        n = 9;
    }
    put_raw(w, head, n);
}

void cbor_put_uint(struct cbor_writer *w, uint64_t value) { put_head(w, 0, value); }
void cbor_put_int(struct cbor_writer *w, int64_t value)
{
    if (value >= 0) put_head(w, 0, (uint64_t)value);
    else put_head(w, 1, (uint64_t)(-1 - value));
}
void cbor_put_bytes(struct cbor_writer *w, const void *data, size_t length)
{
    put_head(w, 2, length);
    put_raw(w, data, length);
}
void cbor_put_text(struct cbor_writer *w, const char *text)
{
    size_t length = strlen(text);
    put_head(w, 3, length);
    put_raw(w, text, length);
}
void cbor_put_bool(struct cbor_writer *w, bool value)
{
    uint8_t b = value ? 0xf5 : 0xf4;
    put_raw(w, &b, 1);
}
void cbor_put_map(struct cbor_writer *w, size_t pairs) { put_head(w, 5, pairs); }
void cbor_put_array(struct cbor_writer *w, size_t items) { put_head(w, 4, items); }

struct reader { const uint8_t *p, *end; };

/* Reads one item head. *indefinite set for 0x9f/0xbf. Returns false on error. */
static bool read_head(struct reader *r, uint8_t *major, uint64_t *value, bool *indefinite)
{
    if (r->p >= r->end) return false;
    uint8_t first = *r->p++;
    unsigned info = first & 31;
    *major = first >> 5;
    *indefinite = false;
    if (info < 24) {
        *value = info;
        return true;
    }
    if (info == 31) {
        if (*major != 4 && *major != 5) return false;
        *indefinite = true;
        *value = 0;
        return true;
    }
    if (info > 27) return false;
    size_t n = (size_t)1 << (info - 24);
    if ((size_t)(r->end - r->p) < n) return false;
    uint64_t v = 0;
    for (size_t i = 0; i < n; i++) v = v << 8 | *r->p++;
    *value = v;
    return true;
}

static bool is_break(const struct reader *r) { return r->p < r->end && *r->p == 0xff; }

static bool read_value(struct reader *r, struct cbor_value *out, unsigned depth)
{
    uint8_t major;
    uint64_t value;
    bool indefinite;
    if (depth > 4 || !read_head(r, &major, &value, &indefinite)) return false;
    if (out) memset(out, 0, sizeof *out);
    switch (major) {
    case 0:
    case 1:
        if (out) { out->type = major ? CBOR_NINT : CBOR_UINT; out->u = value; }
        return true;
    case 2:
    case 3:
        if (value > (uint64_t)(r->end - r->p)) return false;
        if (out) { out->type = major == 2 ? CBOR_BYTES : CBOR_TEXT; out->ptr = r->p; out->len = (size_t)value; }
        r->p += value;
        return true;
    case 4:
    case 5: {
        if (out) { out->type = major == 4 ? CBOR_ARRAY : CBOR_MAP; out->u = value; }
        unsigned per = major == 5 ? 2 : 1;
        if (indefinite) {
            while (!is_break(r))
                for (unsigned k = 0; k < per; k++)
                    if (!read_value(r, NULL, depth + 1)) return false;
            r->p++;
            return true;
        }
        if (value > (uint64_t)(r->end - r->p)) return false; /* each item >= 1 byte */
        for (uint64_t i = 0; i < value * per; i++)
            if (!read_value(r, NULL, depth + 1)) return false;
        return true;
    }
    case 7:
        if (value == 20 || value == 21) {
            if (out) { out->type = CBOR_BOOL; out->u = value == 21; }
            return true;
        }
        if (value == 22) {
            if (out) out->type = CBOR_NULL;
            return true;
        }
        return false;
    default:
        return false; /* tags unsupported */
    }
}

static int find(const uint8_t *buf, size_t length, const char *key, struct cbor_value *out)
{
    struct reader r = { buf, buf + length };
    uint8_t major;
    uint64_t pairs;
    bool indefinite;
    int found = 1;
    size_t key_len = key ? strlen(key) : 0;
    if (!read_head(&r, &major, &pairs, &indefinite) || major != 5) return -1;
    if (!indefinite && pairs > length) return -1;
    for (uint64_t i = 0; indefinite ? !is_break(&r) : i < pairs; i++) {
        struct cbor_value k, v;
        if (!read_value(&r, &k, 1) || !read_value(&r, &v, 1)) return -1;
        if (key && found == 1 && k.type == CBOR_TEXT && k.len == key_len &&
            !memcmp(k.ptr, key, key_len)) {
            if (out) *out = v;
            found = 0;
        }
    }
    if (indefinite) {
        if (!is_break(&r)) return -1;
        r.p++;
    }
    return r.p == r.end ? found : -1;
}

int cbor_map_find(const uint8_t *buf, size_t length, const char *key, struct cbor_value *out)
{
    return find(buf, length, key, out);
}

bool cbor_map_valid(const uint8_t *buf, size_t length)
{
    return find(buf, length, NULL, NULL) >= 0;
}
