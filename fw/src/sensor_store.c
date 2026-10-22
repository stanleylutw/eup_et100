#include "sensor_store.h"
#include "integrity.h"
#include <string.h>

/* Record: "EP" ver state | generation | package_length | header[256] | crc32 */
static void put32(uint8_t *p, uint32_t v)
{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int save(struct sensor_store *s, enum sensor_store_state state, uint32_t generation,
                const uint8_t *header, uint32_t length)
{
    uint8_t r[SENSOR_STORE_RECORD_SIZE] = { 'E', 'P', 1, (uint8_t)state };
    put32(r + 4, generation);
    put32(r + 8, length);
    if (header) memcpy(r + 12, header, SENSOR_PKG_HEADER_SIZE);
    put32(r + 268, installer_crc32(r, 268));
    if (!s->ops || s->ops->save(r, sizeof r)) return -1;
    return 0;
}

void sensor_store_init(struct sensor_store *s, const struct sensor_store_ops *ops)
{
    uint8_t r[SENSOR_STORE_RECORD_SIZE];
    memset(s, 0, sizeof *s);
    s->ops = ops;
    s->status.state = SENSOR_STORE_EMPTY;
    if (!ops || !ops->load) return;
    size_t n = ops->load(r, sizeof r);
    if (!n) return;                                       /* absent: EMPTY, generation 0 */
    s->status.state = SENSOR_STORE_INVALID;               /* present but unusable */
    if (n != sizeof r || r[0] != 'E' || r[1] != 'P' || r[2] != 1 ||
        installer_crc32(r, 268) != get32(r + 268))
        return;
    s->status.generation = get32(r + 4);
    if (r[3] == SENSOR_STORE_VALID &&
        !sensor_pkg_parse_header(r + 12, get32(r + 8), &s->status.info))
        s->status.state = SENSOR_STORE_VALID;
}

int sensor_store_begin(struct sensor_store *s)
{
    uint32_t generation = s->status.generation + 1;
    if (!generation) generation = 1;
    struct sensor_store_status next = { SENSOR_STORE_INVALID, generation, { 0 } };
    if (save(s, SENSOR_STORE_INVALID, generation, NULL, 0)) return -1;
    s->status = next;
    return 0;
}

int sensor_store_commit(struct sensor_store *s, const uint8_t header[SENSOR_PKG_HEADER_SIZE],
                        uint32_t package_length)
{
    struct sensor_store_status next = { SENSOR_STORE_VALID, s->status.generation, { 0 } };
    if (sensor_pkg_parse_header(header, package_length, &next.info)) return -1;
    if (save(s, SENSOR_STORE_VALID, next.generation, header, package_length)) return -1;
    s->status = next;
    return 0;
}

int sensor_store_mark_invalid(struct sensor_store *s)
{
    if (save(s, SENSOR_STORE_INVALID, s->status.generation, NULL, 0)) return -1;
    s->status.state = SENSOR_STORE_INVALID;
    memset(&s->status.info, 0, sizeof s->status.info);
    return 0;
}

int sensor_store_remove(struct sensor_store *s)
{
    if (!s->ops || !s->ops->remove || s->ops->remove()) return -1;
    memset(&s->status, 0, sizeof s->status);              /* EMPTY, generation 0 (gateway) */
    s->status.state = SENSOR_STORE_EMPTY;
    return 0;
}
