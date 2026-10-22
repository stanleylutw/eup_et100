#include "smp_serial.h"
#include <string.h>

uint16_t smp_crc16_xmodem(const uint8_t *data, size_t length)
{
    uint16_t crc = 0;
    while (length--) {
        crc ^= (uint16_t)(*data++ << 8);
        for (unsigned i = 0; i < 8; i++)
            crc = crc & 0x8000 ? (uint16_t)(crc << 1 ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static const char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Decode one line; returns decoded bytes or -1. Padding allowed only at the end. */
static long b64_decode(const char *in, size_t n, uint8_t *out, size_t cap)
{
    size_t used = 0;
    if (n % 4) return -1;
    for (size_t i = 0; i < n; i += 4) {
        int v[4];
        unsigned pad = 0;
        for (unsigned k = 0; k < 4; k++) {
            if (in[i + k] == '=') {
                if (i + 4 != n || k < 2) return -1;
                pad++;
                v[k] = 0;
            } else {
                if (pad) return -1;
                v[k] = b64_value(in[i + k]);
                if (v[k] < 0) return -1;
            }
        }
        uint32_t quad = (uint32_t)v[0] << 18 | (uint32_t)v[1] << 12 | (uint32_t)v[2] << 6 | (uint32_t)v[3];
        size_t bytes = 3 - pad;
        if (cap - used < bytes) return -1;
        for (size_t k = 0; k < bytes; k++) out[used++] = (uint8_t)(quad >> (16 - 8 * k));
    }
    return (long)used;
}

void smp_serial_init(struct smp_serial *s)
{
    memset(s, 0, sizeof *s);
}

void smp_serial_expire(struct smp_serial *s, uint32_t now_ms)
{
    uint32_t idle = now_ms - s->last_ms;
    s->now_ms = now_ms;
    if (s->state != SMP_IDLE && idle > SMP_SERIAL_LINE_TIMEOUT_MS) {
        s->state = SMP_IDLE;
        s->line_len = 0;
        s->in_packet = false;
        s->raw_len = 0;
        s->timeouts++;
    } else if (s->in_packet && idle > SMP_SERIAL_PACKET_TIMEOUT_MS) {
        s->in_packet = false;
        s->raw_len = 0;
        s->timeouts++;
    }
}

bool smp_serial_busy(const struct smp_serial *s)
{
    return s->state != SMP_IDLE;
}

static void drop_packet(struct smp_serial *s)
{
    s->in_packet = false;
    s->raw_len = 0;
}

static int finish_line(struct smp_serial *s, const uint8_t **packet, size_t *length)
{
    s->state = SMP_IDLE;
    if (s->line_bad) {
        s->overflows++;
        drop_packet(s);
        return 1;
    }
    long n = b64_decode(s->line, s->line_len, s->raw + s->raw_len,
                        sizeof s->raw - s->raw_len);
    if (n < 0) {
        s->overflows++;
        drop_packet(s);
        return 1;
    }
    s->raw_len += (size_t)n;
    if (s->raw_len < 2) return 1;
    size_t total = (size_t)s->raw[0] << 8 | s->raw[1];
    if (total < 2 + 8 || total > SMP_SERIAL_MAX_PACKET + 2) {
        s->overflows++;
        drop_packet(s);
        return 1;
    }
    if (s->raw_len - 2 < total) return 1;     /* wait for continuation lines */
    if (s->raw_len - 2 > total) {             /* trailing garbage */
        drop_packet(s);
        return 1;
    }
    size_t data_len = total - 2;
    uint16_t crc = (uint16_t)(s->raw[2 + data_len] << 8 | s->raw[3 + data_len]);
    s->in_packet = false;
    s->raw_len = 0;
    if (crc != smp_crc16_xmodem(s->raw + 2, data_len)) {
        s->crc_errors++;
        return 1;
    }
    s->frames++;
    *packet = s->raw + 2;
    *length = data_len;
    return 2;
}

int smp_serial_feed(struct smp_serial *s, uint8_t byte, const uint8_t **packet, size_t *length)
{
    if (s->state != SMP_IDLE || byte == 0x06 || byte == 0x04) s->last_ms = s->now_ms;
    switch (s->state) {
    case SMP_IDLE:
        if (byte == 0x06 || byte == 0x04) {
            s->lead = byte;
            s->state = SMP_LEAD;
            return 1;
        }
        return 0;
    case SMP_LEAD:
        if ((s->lead == 0x06 && byte == 0x09) || (s->lead == 0x04 && byte == 0x14)) {
            if (s->lead == 0x06) {         /* start: discard any unfinished packet */
                s->in_packet = true;
                s->raw_len = 0;
            }
            s->line_bad = !s->in_packet;   /* continuation without start */
            s->line_len = 0;
            s->state = SMP_LINE;
            return 1;
        }
        s->state = SMP_IDLE;
        return 1;                          /* stray lead byte: swallow */
    case SMP_LINE:
        if (byte == 0x06 || byte == 0x04) {   /* never base64: a new frame starts, */
            s->lead = byte;                   /* drop the line a dead client left */
            s->state = SMP_LEAD;
            s->overflows++;
            return 1;
        }
        if (byte == '\n') return finish_line(s, packet, length);
        if (byte == '\r') return 1;
        if (s->line_len >= sizeof s->line) s->line_bad = true;
        else s->line[s->line_len++] = (char)byte;
        return 1;
    }
    return 0;
}

int smp_serial_encode(const uint8_t *packet, size_t length, smp_serial_sink sink, void *context)
{
    if (length > SMP_SERIAL_MAX_PACKET) return -1;
    uint8_t raw[SMP_SERIAL_MAX_PACKET + 4];
    size_t total = length + 2;
    raw[0] = (uint8_t)(total >> 8);
    raw[1] = (uint8_t)total;
    memcpy(raw + 2, packet, length);
    uint16_t crc = smp_crc16_xmodem(packet, length);
    raw[2 + length] = (uint8_t)(crc >> 8);
    raw[3 + length] = (uint8_t)crc;
    size_t raw_len = length + 4, pos = 0;
    bool first = true;
    while (pos < raw_len || first) {
        char line[2 + SMP_SERIAL_TX_LINE + 1];
        size_t used = 0;
        line[used++] = first ? 0x06 : 0x04;
        line[used++] = first ? 0x09 : 0x14;
        /* 124 chars = 31 quads = 93 bytes: every line is a whole number of quads. */
        for (size_t quads = 0; quads < SMP_SERIAL_TX_LINE / 4 && pos < raw_len; quads++) {
            uint32_t v = 0;
            size_t take = raw_len - pos < 3 ? raw_len - pos : 3;
            for (size_t k = 0; k < 3; k++) v = v << 8 | (k < take ? raw[pos + k] : 0);
            pos += take;
            line[used++] = b64[v >> 18 & 63];
            line[used++] = b64[v >> 12 & 63];
            line[used++] = take > 1 ? b64[v >> 6 & 63] : '=';
            line[used++] = take > 2 ? b64[v & 63] : '=';
        }
        line[used++] = '\n';
        sink(line, used, context);
        first = false;
    }
    return 0;
}
