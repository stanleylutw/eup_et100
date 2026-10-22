#include "sensor_smp_client.h"
#include "cbor_min.h"
#include "smp_server.h"
#include <errno.h>
#include <string.h>

#define FRAME_MAX 1024u
#define RESPONSE_MAX 512u
#define IMG_STATE_ID 0u
#define IMG_UPLOAD_ID 1u
#define IMG_ERASE_ID 5u
#define OS_RESET_ID 5u

static const struct sensor_smp_link *link;
static uint8_t frame[FRAME_MAX];
static uint8_t image_data[SENSOR_SMP_CHUNK_MAX];
static uint8_t smp_seq;
/* RX: filled from the BLE callback only while armed. */
static uint8_t response[RESPONSE_MAX];
static volatile uint16_t response_len;
static volatile unsigned response_done, response_error, armed;

void sensor_smp_client_begin(const struct sensor_smp_link *l) { link = l; armed = 0; }
void sensor_smp_client_end(void) { armed = 0; link = NULL; }

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }

void sensor_smp_client_rx(const uint8_t *data, size_t length)
{
    if (!armed || response_done) return;
    uint16_t used = response_len;
    if (used > RESPONSE_MAX || length > RESPONSE_MAX - used) {
        response_error = 1; response_done = 1; return;
    }
    memcpy(response + used, data, length);
    used = (uint16_t)(used + length);
    response_len = used;
    if (used < SMP_HEADER_SIZE) return;
    size_t total = SMP_HEADER_SIZE + be16(response + 2);
    if (total > RESPONSE_MAX || used > total) { response_error = 1; response_done = 1; }
    else if (used == total) response_done = 1;
}

static size_t finish_header(uint8_t op, uint16_t group, uint8_t id, size_t payload_len)
{
    frame[0] = op;
    frame[1] = 0;
    frame[2] = (uint8_t)(payload_len >> 8);
    frame[3] = (uint8_t)payload_len;
    frame[4] = (uint8_t)(group >> 8);
    frame[5] = (uint8_t)group;
    frame[6] = smp_seq++;
    frame[7] = id;
    return SMP_HEADER_SIZE + payload_len;
}

static int write_frame(size_t len)
{
    uint16_t mtu = link->mtu();
    size_t part = mtu > 3u ? mtu - 3u : 20u;
    for (size_t offset = 0; offset < len;) {
        size_t n = len - offset < part ? len - offset : part;
        /* The stack may refuse a write command while its TX buffers are full. */
        uint32_t start = link->now_ms();
        int err;
        while ((err = link->write(frame + offset, n)) != 0) {
            if ((uint32_t)(link->now_ms() - start) >= 2000u) return -EIO;
            static volatile unsigned never;
            int w = link->wait(&never, 5);
            if (w != -ETIMEDOUT) return w;
        }
        offset += n;
    }
    return 0;
}

static int send_request(size_t frame_len, uint16_t group, uint8_t id, uint8_t response_op,
                        uint32_t timeout_ms)
{
    if (!link) return -ENOTCONN;
    uint8_t seq = frame[6];
    armed = 0;
    response_len = 0;
    response_done = response_error = 0;
    armed = 1;
    int err = write_frame(frame_len);
    if (!err) err = link->wait(&response_done, timeout_ms);
    armed = 0;
    if (err) return err;
    if (response_error) return -EMSGSIZE;
    if (response_len < SMP_HEADER_SIZE || response[0] != response_op || response[6] != seq ||
        be16(response + 4) != group || response[7] != id ||
        be16(response + 2) + SMP_HEADER_SIZE != response_len)
        return -EPROTO;
    return 0;
}

static const uint8_t *payload(size_t *len)
{
    *len = be16(response + 2);
    return response + SMP_HEADER_SIZE;
}

static int parse_response_rc(void)
{
    size_t len;
    const uint8_t *p = payload(&len);
    struct cbor_value rc;
    if (!len) return 0;
    int found = cbor_map_find(p, len, "rc", &rc);
    if (found < 0) return -EBADMSG;
    if (found == 0 && (rc.type != CBOR_UINT || rc.u != 0)) return -EIO;
    return 0;
}

static int encode_upload(uint32_t offset, uint32_t image_length, const uint8_t sha256[32],
                         const uint8_t *data, size_t data_len, size_t *frame_len)
{
    struct cbor_writer w;
    cbor_writer_init(&w, frame + SMP_HEADER_SIZE, sizeof frame - SMP_HEADER_SIZE);
    cbor_put_map(&w, offset == 0 ? 5 : 3);
    cbor_put_text(&w, "image"); cbor_put_uint(&w, 0);
    cbor_put_text(&w, "data"); cbor_put_bytes(&w, data, data_len);
    cbor_put_text(&w, "off"); cbor_put_uint(&w, offset);
    if (offset == 0) {
        cbor_put_text(&w, "len"); cbor_put_uint(&w, image_length);
        cbor_put_text(&w, "sha"); cbor_put_bytes(&w, sha256, 32);
    }
    if (w.error) return -ENOMEM;
    *frame_len = finish_header(SMP_OP_WRITE, SMP_GROUP_IMG, IMG_UPLOAD_ID, w.len);
    return 0;
}

static int parse_upload_response(uint32_t *offset)
{
    size_t len;
    const uint8_t *p = payload(&len);
    struct cbor_value rc, off;
    if (!len) return -EIO;
    int has_rc = cbor_map_find(p, len, "rc", &rc);
    if (has_rc < 0 || (has_rc == 0 && (rc.type != CBOR_UINT || rc.u != 0))) return -EIO;
    if (cbor_map_find(p, len, "off", &off) != 0 || off.type != CBOR_UINT || off.u > UINT32_MAX)
        return -EIO;
    *offset = (uint32_t)off.u;
    return 0;
}

static int erase_slot(void)
{
    struct cbor_writer w;
    cbor_writer_init(&w, frame + SMP_HEADER_SIZE, sizeof frame - SMP_HEADER_SIZE);
    cbor_put_map(&w, 1);
    cbor_put_text(&w, "slot"); cbor_put_uint(&w, 1);
    if (w.error) return -ENOMEM;
    size_t n = finish_header(SMP_OP_WRITE, SMP_GROUP_IMG, IMG_ERASE_ID, w.len);
    int err = send_request(n, SMP_GROUP_IMG, IMG_ERASE_ID, SMP_OP_WRITE_RSP, SENSOR_SMP_ERASE_TIMEOUT_MS);
    return err ? err : parse_response_rc();
}

int sensor_smp_client_upload(uint32_t image_length, const uint8_t sha256[32], size_t chunk,
                             bool allow_resume, bool erase_first, uint32_t *bytes_done,
                             sensor_smp_progress_fn progress, sensor_smp_erase_fn erase_cb, void *ctx)
{
    if (!link || !sha256 || !bytes_done || !image_length || !chunk || chunk > SENSOR_SMP_CHUNK_MAX)
        return -EINVAL;
    *bytes_done = 0;
    if (erase_first) {
        uint32_t started = link->now_ms();
        int err = erase_slot();
        if (erase_cb) erase_cb(err, link->now_ms() - started, ctx);
        if (err) return err;
    }
    uint32_t deadline_start = link->now_ms(), offset = 0;
    bool first_response = true;
    if (link->mtu() < 23u) return -EMSGSIZE;
    while (offset < image_length) {
        if ((uint32_t)(link->now_ms() - deadline_start) >= SENSOR_SMP_UPLOAD_DEADLINE_MS) return -ETIMEDOUT;
        size_t data_len = image_length - offset < chunk ? image_length - offset : chunk, frame_len;
        if (link->read_image(offset, image_data, data_len)) return -EIO;
        int err = encode_upload(offset, image_length, sha256, image_data, data_len, &frame_len);
        if (!err) err = send_request(frame_len, SMP_GROUP_IMG, IMG_UPLOAD_ID, SMP_OP_WRITE_RSP,
                                     SENSOR_SMP_COMMAND_TIMEOUT_MS);
        if (err) return err;
        uint32_t previous = offset;
        err = parse_upload_response(&offset);
        if (!err && offset <= previous) return -EIO;
        if (!err && offset > previous + data_len && (!allow_resume || !first_response)) return -EOVERFLOW;
        first_response = false;
        if (err) return err;
        if (offset > image_length) return -EOVERFLOW;
        *bytes_done = offset;
        if (progress) progress(offset, ctx);
    }
    return 0;
}

static int parse_image_state(struct sensor_smp_image_state *images, size_t capacity, size_t *count)
{
    size_t len;
    const uint8_t *p = payload(&len);
    struct cbor_value list;
    *count = 0;
    if (cbor_map_find(p, len, "images", &list) != 0 || list.type != CBOR_ARRAY) return -EBADMSG;
    for (size_t i = 0;; i++) {
        const uint8_t *item;
        size_t item_len;
        int rc = cbor_array_item(&list, i, &item, &item_len);
        if (rc == 1) return 0;
        if (rc) return -EBADMSG;
        struct cbor_value version, hash, slot, flag;
        if (cbor_map_find(item, item_len, "version", &version) != 0 || version.type != CBOR_TEXT ||
            !version.len || cbor_map_find(item, item_len, "hash", &hash) != 0 ||
            hash.type != CBOR_BYTES || hash.len != 32 ||
            cbor_map_find(item, item_len, "slot", &slot) != 0 || slot.type != CBOR_UINT ||
            slot.u >= UINT32_MAX)
            return -EBADMSG;
        if (*count < capacity) {
            struct sensor_smp_image_state *s = &images[(*count)++];
            memcpy(s->hash, hash.ptr, 32);
            s->slot = (uint32_t)slot.u;
            s->active = cbor_map_find(item, item_len, "active", &flag) == 0 && flag.type == CBOR_BOOL && flag.u;
            s->pending = cbor_map_find(item, item_len, "pending", &flag) == 0 && flag.type == CBOR_BOOL && flag.u;
            s->confirmed = cbor_map_find(item, item_len, "confirmed", &flag) == 0 && flag.type == CBOR_BOOL && flag.u;
        }
    }
}

static int state_request(const uint8_t hash[32], bool write, struct sensor_smp_image_state *images,
                         size_t capacity, size_t *count)
{
    struct cbor_writer w;
    cbor_writer_init(&w, frame + SMP_HEADER_SIZE, sizeof frame - SMP_HEADER_SIZE);
    if (write) {
        cbor_put_map(&w, 2);
        cbor_put_text(&w, "confirm"); cbor_put_bool(&w, false);
        cbor_put_text(&w, "hash"); cbor_put_bytes(&w, hash, 32);
    } else {
        cbor_put_map(&w, 0);
    }
    if (w.error) return -ENOMEM;
    size_t n = finish_header(write ? SMP_OP_WRITE : SMP_OP_READ, SMP_GROUP_IMG, IMG_STATE_ID, w.len);
    int err = send_request(n, SMP_GROUP_IMG, IMG_STATE_ID, write ? SMP_OP_WRITE_RSP : SMP_OP_READ_RSP,
                           SENSOR_SMP_COMMAND_TIMEOUT_MS);
    return err ? err : parse_image_state(images, capacity, count);
}

int sensor_smp_client_image_state(struct sensor_smp_image_state *images, size_t capacity, size_t *count)
{
    if (!images || !capacity || !count) return -EINVAL;
    return state_request(NULL, false, images, capacity, count);
}

int sensor_smp_client_image_test(const uint8_t hash[32])
{
    struct sensor_smp_image_state images[3];
    size_t count = 0;
    if (!hash) return -EINVAL;
    int err = state_request(hash, true, images, 3, &count);
    if (err) return err;
    for (size_t i = 0; i < count; i++)
        if (images[i].pending && !memcmp(images[i].hash, hash, 32)) return 0;
    return -EBADMSG;
}

int sensor_smp_client_os_reset(void)
{
    size_t n = finish_header(SMP_OP_WRITE, SMP_GROUP_OS, OS_RESET_ID, 0);
    int err = send_request(n, SMP_GROUP_OS, OS_RESET_ID, SMP_OP_WRITE_RSP, SENSOR_SMP_COMMAND_TIMEOUT_MS);
    /* A target may reset before its response reaches us. */
    return err == -ENOTCONN ? 0 : err ? err : parse_response_rc();
}
