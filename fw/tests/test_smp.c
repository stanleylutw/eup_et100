/* v0.8.5 host tests: CBOR subset, SMP serial framing, SMP server, OTA manager.
 * Usage: test_smp <et100.signed.bin> */
#include "cbor_min.h"
#include "smp_serial.h"
#include "smp_server.h"
#include "ota_manager.h"
#include "fake_nor.h"
#include "smp_ble.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- CBOR ---------- */
static void test_cbor(void)
{
    uint8_t buf[256];
    struct cbor_writer w;
    cbor_writer_init(&w, buf, sizeof buf);
    cbor_put_map(&w, 6);
    cbor_put_text(&w, "off");  cbor_put_uint(&w, 70000);
    cbor_put_text(&w, "data"); cbor_put_bytes(&w, "\x01\x02\x03", 3);
    cbor_put_text(&w, "neg");  cbor_put_int(&w, -300);
    cbor_put_text(&w, "ok");   cbor_put_bool(&w, true);
    cbor_put_text(&w, "arr");  cbor_put_array(&w, 2); cbor_put_uint(&w, 1); cbor_put_text(&w, "x");
    cbor_put_text(&w, "big");  cbor_put_uint(&w, 0x123456789ull);
    assert(!w.error && cbor_map_valid(buf, w.len));
    struct cbor_value v;
    assert(!cbor_map_find(buf, w.len, "off", &v) && v.type == CBOR_UINT && v.u == 70000);
    assert(!cbor_map_find(buf, w.len, "data", &v) && v.type == CBOR_BYTES && v.len == 3 && v.ptr[2] == 3);
    assert(!cbor_map_find(buf, w.len, "neg", &v) && v.type == CBOR_NINT && v.u == 299);
    assert(!cbor_map_find(buf, w.len, "ok", &v) && v.type == CBOR_BOOL && v.u == 1);
    assert(!cbor_map_find(buf, w.len, "arr", &v) && v.type == CBOR_ARRAY && v.u == 2);
    assert(!cbor_map_find(buf, w.len, "big", &v) && v.u == 0x123456789ull);
    assert(cbor_map_find(buf, w.len, "nope", &v) == 1);
    for (size_t cut = 0; cut < w.len; cut++) assert(cbor_map_find(buf, cut, "off", &v) == -1);
    const uint8_t indef[] = { 0xbf, 0x61, 'a', 0x01, 0x61, 'b', 0x9f, 0x02, 0xff, 0xff };
    assert(!cbor_map_find(indef, sizeof indef, "a", &v) && v.u == 1);
    assert(!cbor_map_find(indef, sizeof indef, "b", &v) && v.type == CBOR_ARRAY);
    const uint8_t not_map[] = { 0x01 };
    assert(cbor_map_find(not_map, 1, "a", &v) == -1);
    cbor_writer_init(&w, buf, 4);
    cbor_put_text(&w, "too long");
    assert(w.error);
    puts("PASS cbor");
}

/* ---------- framing ---------- */
static uint8_t lines[8192];
static size_t lines_len;
static void sink(const char *data, size_t length, void *context)
{
    (void)context;
    assert(lines_len + length <= sizeof lines);
    for (size_t i = 0; i < length; i++) {
        if (i >= 2 && i + 1 < length) assert(data[i] != '\n');
    }
    assert(length <= 127 && data[length - 1] == '\n');
    memcpy(lines + lines_len, data, length);
    lines_len += length;
}
static void test_framing(void)
{
    struct smp_serial s;
    uint8_t packet[SMP_SERIAL_MAX_PACKET];
    for (size_t size = 8; size <= SMP_SERIAL_MAX_PACKET; size += 97) {
        for (size_t i = 0; i < size; i++) packet[i] = (uint8_t)(i * 31 + size);
        lines_len = 0;
        assert(!smp_serial_encode(packet, size, sink, NULL));
        smp_serial_init(&s);
        const char *junk = "BLEGW$GCHK;J082\r\n";
        for (const char *j = junk; *j; j++) {
            const uint8_t *out; size_t n;
            assert(smp_serial_feed(&s, (uint8_t)*j, &out, &n) == 0);
        }
        int got = 0;
        for (size_t i = 0; i < lines_len; i++) {
            const uint8_t *out; size_t n;
            int r = smp_serial_feed(&s, lines[i], &out, &n);
            assert(r != 0);
            if (r == 2) { assert(n == size && !memcmp(out, packet, size)); got++; }
        }
        assert(got == 1 && s.frames == 1);
    }
    /* corrupted CRC: flip a base64 char inside the first line */
    for (size_t i = 0; i < 40; i++) packet[i] = (uint8_t)i;
    lines_len = 0;
    smp_serial_encode(packet, 40, sink, NULL);
    lines[10] = lines[10] == 'A' ? 'B' : 'A';
    smp_serial_init(&s);
    for (size_t i = 0; i < lines_len; i++) { const uint8_t *o; size_t n; assert(smp_serial_feed(&s, lines[i], &o, &n) != 2); }
    assert(s.frames == 0);
    /* continuation without start is dropped */
    smp_serial_init(&s);
    const char cont[] = "\x04\x14" "AAAA\n";
    for (size_t i = 0; i < sizeof cont - 1; i++) { const uint8_t *o; size_t n; assert(smp_serial_feed(&s, (uint8_t)cont[i], &o, &n) == 1); }
    assert(s.frames == 0 && !smp_serial_busy(&s));
    /* a client killed mid-line: the next start marker must still be decoded */
    for (size_t i = 0; i < 40; i++) packet[i] = (uint8_t)(i + 7);
    lines_len = 0;
    smp_serial_encode(packet, 40, sink, NULL);
    smp_serial_init(&s);
    const char dead[] = "\x06\x09" "AAAAAAAA";             /* no newline */
    for (size_t i = 0; i < sizeof dead - 1; i++) { const uint8_t *o; size_t n; smp_serial_feed(&s, (uint8_t)dead[i], &o, &n); }
    int ok = 0;
    for (size_t i = 0; i < lines_len; i++) { const uint8_t *o; size_t n; ok += smp_serial_feed(&s, lines[i], &o, &n) == 2; }
    assert(ok == 1 && s.frames == 1);
    assert(smp_crc16_xmodem((const uint8_t *)"123456789", 9) == 0x31c3);
    puts("PASS framing");
}

/* ---------- fake device ---------- */
static uint8_t *internal;           /* 2 MB internal flash image */
static int in_read(uint32_t off, void *data, size_t n)
{
    if (off > 0x200000 || n > 0x200000 - off) return -1;
    memcpy(data, internal + off, n);
    return 0;
}
static int in_erase(uint32_t off, size_t n)
{
    if (off < BOOT_STATE_BASE || (off & 4095) || (n & 4095) || n > 0x200000 - off) return -1;
    memset(internal + off, 0xff, n);
    return 0;
}
static int in_program(uint32_t off, const void *data, size_t n)
{
    if (off < BOOT_STATE_BASE || n > 0x200000 - off) return -1;
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i++) internal[off + i] &= p[i];
    return 0;
}
static const struct internal_port in_port = { in_read, in_erase, in_program };
static struct fake_nor nor;
static int prepares;
static uint32_t clock_ms;
static uint32_t fake_now(void) { return clock_ms; }
static int prepare(void) { prepares++; return 0; }

static uint8_t *running, *candidate;
static size_t image_len;
static struct ota_env env;
static struct ota_manager mgr;
static struct smp_ops ops;
static uint8_t seq;

static void reboot(void)
{
    ota_manager_init(&mgr, &env);
    ota_manager_smp_ops(&mgr, &ops);
}

/* Send one request; returns response CBOR payload (static buffer). */
static uint8_t rsp[SMP_SERIAL_MAX_PACKET];
static size_t rsp_len;
static const uint8_t *call(uint8_t op, uint16_t group, uint8_t id, const uint8_t *cbor, size_t n, bool *reset)
{
    uint8_t req[SMP_SERIAL_MAX_PACKET];
    req[0] = (uint8_t)(1 << 3 | op);  /* SMP v2 */
    req[1] = 0;
    req[2] = (uint8_t)(n >> 8); req[3] = (uint8_t)n;
    req[4] = (uint8_t)(group >> 8); req[5] = (uint8_t)group;
    req[6] = ++seq; req[7] = id;
    memcpy(req + 8, cbor, n);
    bool r;
    rsp_len = smp_server_process(&ops, req, n + 8, rsp, sizeof rsp, &r);
    assert(rsp_len >= 8);
    assert((rsp[0] & 7) == op + 1 && (rsp[0] >> 3 & 3) == 1 && rsp[6] == seq && rsp[7] == id);
    assert(((size_t)rsp[2] << 8 | rsp[3]) == rsp_len - 8);
    if (reset) *reset = r;
    return rsp + 8;
}
static uint64_t field(const uint8_t *m, const char *key)
{
    struct cbor_value v;
    if (cbor_map_find(m, rsp_len - 8, key, &v)) return UINT64_MAX;
    return v.u;
}

static uint32_t upload_packet(const uint8_t *image, uint32_t off, size_t n, bool first)
{
    uint8_t buf[SMP_SERIAL_MAX_PACKET];
    struct cbor_writer w;
    cbor_writer_init(&w, buf, sizeof buf);
    cbor_put_map(&w, first ? 5 : 2);
    cbor_put_text(&w, "off");
    cbor_put_uint(&w, off);
    cbor_put_text(&w, "data");
    cbor_put_bytes(&w, image + off, n);
    if (first) {
        uint8_t sha[32];
        mbedtls_sha256_ret(image, image_len, sha, 0);
        cbor_put_text(&w, "image"); cbor_put_uint(&w, 0);
        cbor_put_text(&w, "len");   cbor_put_uint(&w, image_len);
        cbor_put_text(&w, "sha");   cbor_put_bytes(&w, sha, 32);
    }
    assert(!w.error);
    const uint8_t *m = call(SMP_OP_WRITE, SMP_GROUP_IMG, 1, buf, w.len, NULL);
    uint64_t rc = field(m, "rc");
    if (rc != 0) return 0x80000000u | (uint32_t)rc;
    return (uint32_t)field(m, "off");
}

/* Upload like smpclient: first packet carries len/sha; resume via returned off.
 * reboot_after: simulate an MCU reset after this many packets (0 = never). */
static unsigned upload(const uint8_t *image, unsigned reboot_after)
{
    unsigned count = 0;
    bool rebooted = false;
    uint32_t off = upload_packet(image, 0, 300, true);
    assert(!(off & 0x80000000u) && off == 300);
    while (off < image_len) {
        size_t n = 200 + (count * 37) % 500;
        if (n > image_len - off) n = image_len - off;
        if (reboot_after && !rebooted && ++count == reboot_after) {
            reboot();                                   /* MCU reset: RAM buffer lost */
            rebooted = true;
            off = upload_packet(image, 0, 300, true);   /* client restarts at 0 */
            assert(!(off & 0x80000000u) && off % OTA_JOURNAL_CHUNK_SIZE == 0 && off > 0);
            continue;
        }
        count += !reboot_after;
        uint32_t next = upload_packet(image, off, n, false);
        assert(!(next & 0x80000000u) && next == off + n);
        off = next;
    }
    return count;
}

static void write_state(enum boot_kind kind, const uint8_t *image_for_hash)
{
    struct boot_record s;
    memset(&s, 0, sizeof s);
    s.kind = kind;
    s.length = (uint32_t)image_len;
    mbedtls_sha256_ret(image_for_hash, image_len, s.hash, 0);
    s.confirmed_length = (uint32_t)image_len;
    mbedtls_sha256_ret(running, image_len, s.confirmed_hash, 0);
    assert(!boot_state_write(&in_port, &s));
}

static void tlv_hash(const uint8_t *image, uint8_t out[32])
{
    struct image_info info;
    assert(!mcuboot_image_verify(image, image_len, &info));
    memcpy(out, info.sha256, 32);
}

static void state_write(const uint8_t *hash, int confirm, uint64_t expect_rc)
{
    uint8_t buf[64];
    struct cbor_writer w;
    cbor_writer_init(&w, buf, sizeof buf);
    cbor_put_map(&w, (hash ? 1 : 0) + (confirm >= 0 ? 1 : 0));
    if (hash) { cbor_put_text(&w, "hash"); cbor_put_bytes(&w, hash, 32); }
    if (confirm >= 0) { cbor_put_text(&w, "confirm"); cbor_put_bool(&w, confirm); }
    const uint8_t *m = call(SMP_OP_WRITE, SMP_GROUP_IMG, 0, buf, w.len, NULL);
    uint64_t rc = field(m, "rc");
    if (expect_rc) assert(rc == expect_rc);
    else assert(rc == UINT64_MAX || rc == 0);
}

static void rehash(uint8_t *image)
{
    /* Recompute the MCUboot SHA-256 TLV after a payload edit (TLV is the last 40 bytes). */
    assert(!mbedtls_sha256_ret(image, image_len - 40, image + image_len - 32, 0));
}

static void test_manager(const char *path)
{
    FILE *f = fopen(path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    image_len = (size_t)ftell(f);
    rewind(f);
    running = malloc(image_len);
    candidate = malloc(image_len);
    internal = malloc(0x200000);
    assert(running && candidate && internal);
    assert(fread(running, 1, image_len, f) == image_len);
    fclose(f);
    memset(internal, 0xff, 0x200000);
    memcpy(candidate, running, image_len);
    candidate[0x400] ^= 0x5a;                   /* a different but valid image */
    rehash(candidate);

    fake_nor_create(&nor);
    fake_nor_select(&nor);
    env.nor = &nor.port;
    env.nor_prepare = prepare;
    env.internal = &in_port;
    env.app_slot = running;
    env.source = OTA_RS232;
    env.now_ms = fake_now;
    reboot();
    write_state(BOOT_CONFIRMED, running);

    /* OS group */
    bool reset;
    const uint8_t echo[] = { 0xa1, 0x61, 'd', 0x62, 'h', 'i' };
    const uint8_t *m = call(SMP_OP_WRITE, SMP_GROUP_OS, 0, echo, sizeof echo, NULL);
    struct cbor_value v;
    assert(!cbor_map_find(m, rsp_len - 8, "r", &v) && v.len == 2 && !memcmp(v.ptr, "hi", 2));
    const uint8_t empty[] = { 0xa0 };
    m = call(SMP_OP_READ, SMP_GROUP_OS, 6, empty, 1, NULL);
    assert(field(m, "buf_size") == 1024 && field(m, "buf_count") == 1);
    call(SMP_OP_WRITE, SMP_GROUP_OS, 5, empty, 1, &reset);
    assert(reset);
    m = call(SMP_OP_WRITE, SMP_GROUP_OS, 1, empty, 1, &reset);
    assert(!reset && field(m, "rc") == SMP_ENOTSUP);
    m = call(SMP_OP_READ, SMP_GROUP_OS, 8, empty, 1, NULL);
    assert(!cbor_map_find(m, rsp_len - 8, "bootloader", &v) && v.len == 7 && !memcmp(v.ptr, "MCUboot", 7));
    const uint8_t mode_q[] = { 0xa1, 0x65, 'q','u','e','r','y', 0x64, 'm','o','d','e' };
    m = call(SMP_OP_READ, SMP_GROUP_OS, 8, mode_q, sizeof mode_q, NULL);
    assert(field(m, "mode") == 3);
    m = call(SMP_OP_READ, 9, 0, empty, 1, NULL);
    assert(field(m, "rc") == SMP_ENOTSUP);

    /* state read before upload: only slot 0 */
    uint8_t run_hash[32], cand_hash[32];
    tlv_hash(running, run_hash);
    tlv_hash(candidate, cand_hash);
    m = call(SMP_OP_READ, SMP_GROUP_IMG, 0, empty, 1, NULL);
    assert(!cbor_map_find(m, rsp_len - 8, "images", &v) && v.type == CBOR_ARRAY && v.u == 1);
    assert(prepares == 1);

    /* full upload with one simulated reboot, resume from committed offset */
    unsigned packets = upload(candidate, 300);
    (void)packets;
    m = call(SMP_OP_READ, SMP_GROUP_IMG, 0, empty, 1, NULL);
    assert(!cbor_map_find(m, rsp_len - 8, "images", &v) && v.u == 2);
    assert(nor.port.read && !memcmp(nor.bytes + OTA_CANDIDATE_BASE, candidate, image_len));

    /* resync: wrong offset returns the expected one */
    assert(upload_packet(candidate, 100, 10, false) == image_len);

    /* test: wrong hash, then right hash -> PENDING; uploads now refused */
    state_write(run_hash, 0, SMP_ENOENT);
    state_write(cand_hash, 0, 0);
    struct boot_record s;
    uint8_t full[32];
    mbedtls_sha256_ret(candidate, image_len, full, 0);
    assert(!boot_state_read(&in_port, &s) && s.kind == BOOT_PENDING && !memcmp(s.hash, full, 32));
    m = call(SMP_OP_READ, SMP_GROUP_IMG, 0, empty, 1, NULL);   /* running stays confirmed */
    assert(!cbor_map_find(m, rsp_len - 8, "images", &v) && v.u == 2);
    {
        const uint8_t *p = m;
        size_t left = rsp_len - 8;
        const uint8_t key[] = { 0x69, 'c','o','n','f','i','r','m','e','d', 0xf5 };
        int hits = 0;
        for (size_t i = 0; i + sizeof key <= left; i++) hits += !memcmp(p + i, key, sizeof key);
        assert(hits == 1);   /* slot 0 confirmed=true, slot 1 confirmed=false */
    }
    state_write(cand_hash, 0, 0);               /* idempotent */
    assert((upload_packet(candidate, 0, 300, true) & 0x7fffffffu) == SMP_EBADSTATE);
    state_write(NULL, 1, SMP_EBADSTATE);        /* confirm while pending */

    /* trial confirm: running image is the trial */
    write_state(BOOT_TRIAL, running);
    reboot();
    state_write(cand_hash, 1, SMP_ENOTSUP);     /* no permanent install */
    state_write(NULL, 1, 0);
    assert(!boot_state_read(&in_port, &s) && s.kind == BOOT_CONFIRMED);

    /* corrupted image: payload byte changed without fixing the MCUboot TLV. The
     * journal accepts it (first-packet sha is of these bytes) but candidate
     * verification fails, so it is never listed and cannot be tested. */
    reboot();
    uint8_t *bad = malloc(image_len);
    assert(bad);
    memcpy(bad, candidate, image_len);
    bad[0x800] ^= 1;
    upload(bad, 0);
    state_write(cand_hash, 0, SMP_ECORRUPT);
    m = call(SMP_OP_READ, SMP_GROUP_IMG, 0, empty, 1, NULL);
    assert(!cbor_map_find(m, rsp_len - 8, "images", &v) && v.u == 1);
    free(bad);

    /* single owner across transports: BLE is refused while UART holds the session */
    reboot();
    clock_ms = 1000;
    ota_manager_set_source(&mgr, OTA_RS232);
    assert(upload_packet(candidate, 0, 300, true) == 300);
    ota_manager_set_source(&mgr, OTA_BLE);
    assert((upload_packet(candidate, 0, 300, true) & 0x7fffffffu) == SMP_EBUSY);
    state_write(cand_hash, 0, SMP_EBUSY);
    m = call(SMP_OP_READ, SMP_GROUP_IMG, 0, empty, 1, NULL);  /* reads still served */
    assert(field(m, "rc") == UINT64_MAX);
    clock_ms += OTA_OWNER_HOLD_MS;                            /* UART went quiet: BLE takes over */
    uint32_t resumed = upload_packet(candidate, 0, 300, true);
    assert(!(resumed & 0x80000000u) && resumed == 300);       /* same image: resume, not restart */
    assert(mgr.journal.progress.manifest.source == OTA_BLE);  /* explicit takeover recorded */
    ota_manager_set_source(&mgr, OTA_RS232);
    assert((upload_packet(candidate, 300, 100, false) & 0x7fffffffu) == SMP_EBUSY);
    ota_manager_set_source(&mgr, OTA_BLE);

    /* a fresh good upload replaces the failed session */
    reboot();
    upload(candidate, 0);
    state_write(cand_hash, 0, 0);
    puts("PASS ota manager (upload/resume/resync/test/confirm/corrupt)");
    fake_nor_destroy(&nor);
    free(running); free(candidate); free(internal);
}

static void test_reassembly(void)
{
    static struct smp_reassembly r;
    uint8_t packet[600];
    for (size_t i = 0; i < sizeof packet; i++) packet[i] = (uint8_t)i;
    packet[2] = (uint8_t)((sizeof packet - 8) >> 8);
    packet[3] = (uint8_t)(sizeof packet - 8);
    smp_reassembly_reset(&r);
    size_t off = 0;
    int done = 0;
    while (off < sizeof packet) {                   /* 244-byte writes (MTU 247) */
        size_t n = sizeof packet - off < 244 ? sizeof packet - off : 244;
        done = smp_reassembly_add(&r, packet + off, n, 10);
        assert(done == (off + n == sizeof packet));
        off += n;
    }
    assert(done == 1 && r.length == sizeof packet && !memcmp(r.data, packet, sizeof packet));
    smp_reassembly_reset(&r);
    assert(smp_reassembly_add(&r, packet, 5, 0) == -1);              /* short header */
    assert(smp_reassembly_add(&r, packet, 244, 0) == 0);
    assert(smp_reassembly_add(&r, packet + 244, 400, 100) == -1);    /* overruns header length */
    assert(smp_reassembly_add(&r, packet, 244, 0) == 0);
    assert(smp_reassembly_add(&r, packet, 244, 5000) == 0);          /* stale part dropped, restart */
    assert(r.length == 244);
    uint8_t big[8] = { 0, 0, 0x04, 0x00 };                         /* 8 + 1024 > 1024 */
    smp_reassembly_reset(&r);
    assert(smp_reassembly_add(&r, big, 8, 0) == -1);
    puts("PASS ble reassembly");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    test_cbor();
    test_framing();
    test_reassembly();
    test_manager(argv[1]);
    puts("ALL PASS");
    return 0;
}
