#ifndef ET100_CBOR_MIN_H
#define ET100_CBOR_MIN_H
/* Minimal CBOR (RFC 8949) subset for SMP: uint, negative int, bytes, text,
 * array, map, bool. Writer emits definite lengths only; reader also accepts
 * indefinite-length maps/arrays. No floats, tags or indefinite strings. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct cbor_writer {
    uint8_t *buf;
    size_t cap, len;
    bool error;
};
void cbor_writer_init(struct cbor_writer *w, uint8_t *buf, size_t cap);
void cbor_put_uint(struct cbor_writer *w, uint64_t value);
void cbor_put_int(struct cbor_writer *w, int64_t value);
void cbor_put_bytes(struct cbor_writer *w, const void *data, size_t length);
void cbor_put_text(struct cbor_writer *w, const char *text);
void cbor_put_bool(struct cbor_writer *w, bool value);
void cbor_put_map(struct cbor_writer *w, size_t pairs);
void cbor_put_array(struct cbor_writer *w, size_t items);

enum cbor_type { CBOR_NONE, CBOR_UINT, CBOR_NINT, CBOR_BYTES, CBOR_TEXT,
                 CBOR_ARRAY, CBOR_MAP, CBOR_BOOL, CBOR_NULL };
struct cbor_value {
    enum cbor_type type;
    uint64_t u;          /* UINT; NINT stores -1-n; BOOL 0/1 */
    const uint8_t *ptr;  /* BYTES/TEXT: content; ARRAY/MAP: whole encoded item */
    size_t len;
};
/* Look up a text key in the top-level map of buf. Returns 0 if found,
 * 1 if absent, -1 if the buffer is not a well-formed top-level map. */
int cbor_map_find(const uint8_t *buf, size_t length, const char *key, struct cbor_value *out);
/* Validate that buf is exactly one well-formed top-level map (depth <= 4). */
bool cbor_map_valid(const uint8_t *buf, size_t length);
/* Item index of an ARRAY value (from cbor_map_find): its encoded bytes, so a map
 * item can be searched with cbor_map_find. Returns 0, 1 past the end, -1 malformed. */
int cbor_array_item(const struct cbor_value *array, size_t index, const uint8_t **ptr, size_t *len);
#endif
