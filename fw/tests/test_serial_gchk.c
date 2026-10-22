#include "serial_gchk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char output[20000];
static size_t used;
static uint16_t durable_gtime = ET100_GTIME_DEFAULT;
static int save_fails;
static unsigned save_calls;
static uint16_t durable_cids[4] = {0x0639};
static int save_cids(const uint16_t cids[4], void *context)
{
    (void)context;
    if (save_fails) return -1;
    memcpy(durable_cids, cids, sizeof(durable_cids));
    return 0;
}

static int save_setting(uint16_t seconds, void *context)
{
    (void)context;
    save_calls++;
    if (save_fails)
        return -1;
    durable_gtime = seconds;
    return 0;
}
static void capture(const char *data, size_t length, void *context)
{
    (void)context;
    assert(used + length < sizeof(output));
    memcpy(output + used, data, length);
    used += length;
    output[used] = '\0';
}

static void feed(struct et100_serial_parser *p, const char *text)
{
    while (*text)
        et100_serial_feed(p, (unsigned char)*text++);
}

static void expect(struct et100_serial_parser *p, const char *request,
                   const char *response)
{
    used = 0;
    feed(p, request);
    assert(strcmp(output, response) == 0);
}

int main(void)
{
    struct et100_serial_parser p;
    const char *expected = "AT$PD=BLEGW$GCHK;1;C00102030405;Freqchip;"
        "DEV1.4_fr3068ep-c;0.8.3-dev;0;A1b2\r\n";
    et100_serial_init(&p, "C00102030405", capture, NULL);
    feed(&p, "BLEGW$GC");
    assert(used == 0);
    feed(&p, "HK;A1b2\r");
    assert(used == 0);
    feed(&p, "\n");
    assert(strcmp(output, expected) == 0);
    used = 0;
    feed(&p, "BLEGW$GMAXRPT;Ab12\r\nBLEGW$GCHK;A1b2\r\n");
    assert(strstr(output, "AT$PD=BLEGW$GMAXRPT;0;Ab12\r\n") == output);
    assert(strstr(output, expected));
    used = 0;
    feed(&p, "BLEGW$GCHK;A1b2\nBLEGW$GCHK;abc\r\n");
    feed(&p, "BLEGW$GCHK;A;12\r\nBLEGW$GCHK;A!b2\r\n");
    feed(&p, "AT#AI20000000 DEADBEEF\r\nBLEGW$GCHK;A1b2\rX\n");
    assert(used == 0);
    for (int i = 0; i < 1000; i++) et100_serial_feed(&p, 'X');
    feed(&p, "BLEGW$GCHK;A1b2\r\n");
    assert(used == 0);
    feed(&p, "BLEGW$GCHK;A1b2\r\n");
    assert(strcmp(output, expected) == 0);
    used = 0;
    et100_serial_feed(&p, 0);
    feed(&p, "BLEGW$GCHK;A1b2\r\n");
    assert(used == 0);
    for (int i = 0; i < 100; i++) feed(&p, "BLEGW$GCHK;A1b2\r\n");
    assert(used == 100 * strlen(expected));
    puts("PASS: split/concatenated frames, tokens, malformed input, overflow recovery, 100 queries");

    expect(&p, "BLEGW$GGTIME;READ\r\n", "AT$PD=BLEGW$GGTIME;0;15;READ\r\n");
    expect(&p, "BLEGW$SGTIME;30;WRIT\r\n", "AT$PD=BLEGW$SGTIME;0;30;WRIT\r\n");
    assert(save_calls == 0);
    et100_serial_set_storage(&p, 1, durable_gtime, save_setting, NULL);
    expect(&p, "BLEGW$GGTIME;READ\r\n", "AT$PD=BLEGW$GGTIME;1;15;READ\r\n");
    expect(&p, "BLEGW$SGTIME;30;Ab12\r\n", "AT$PD=BLEGW$SGTIME;1;30;Ab12\r\n");
    assert(p.gtime == 30 && durable_gtime == 30);
    et100_serial_init(&p, "C00102030405", capture, NULL);
    et100_serial_set_storage(&p, 1, durable_gtime, save_setting, NULL);
    expect(&p, "BLEGW$GGTIME;BOOT\r\n", "AT$PD=BLEGW$GGTIME;1;30;BOOT\r\n");
    unsigned calls = save_calls;
    const char *invalid[] = {"0", "1801", "-1", "+1", "1.5", "abc", "",
                             "4294967296", "9999999999999999999999999", "1;2"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        char request[120], response[150];
        snprintf(request, sizeof(request), "BLEGW$SGTIME;%s;FAIL\r\n", invalid[i]);
        snprintf(response, sizeof(response), "AT$PD=BLEGW$SGTIME;0;%s;FAIL\r\n", invalid[i]);
        expect(&p, request, response);
        assert(p.gtime == 30 && durable_gtime == 30 && save_calls == calls);
    }
    expect(&p, "BLEGW$SGTIME;MISS\r\n", "AT$PD=BLEGW$SGTIME;0;;MISS\r\n");
    expect(&p, "BLEGW$SGTIME;1;MIN1\r\n", "AT$PD=BLEGW$SGTIME;1;1;MIN1\r\n");
    expect(&p, "BLEGW$SGTIME;1800;MAX1\r\n", "AT$PD=BLEGW$SGTIME;1;1800;MAX1\r\n");
    save_fails = 1;
    expect(&p, "BLEGW$SGTIME;60;DISK\r\n", "AT$PD=BLEGW$SGTIME;0;60;DISK\r\n");
    expect(&p, "BLEGW$GGTIME;KEEP\r\n", "AT$PD=BLEGW$GGTIME;1;1800;KEEP\r\n");
    assert(durable_gtime == 1800);
    et100_serial_set_storage(&p, 1, 0, save_setting, NULL);
    assert(p.gtime == ET100_GTIME_DEFAULT);
    puts("PASS: GTIME default, set/query, reload, 1/1800 boundaries, invalid input, storage failures");
    save_fails = 0;
    et100_serial_set_cid_storage(&p, durable_cids, save_cids);
    expect(&p, "BLEGW$GCID;C001\r\n", "AT$PD=BLEGW$GCID;1;0639;;;;C001\r\n");
    expect(&p, "BLEGW$SCID;0639;0dfc;;;C002\r\n", "AT$PD=BLEGW$SCID;1;0639;0DFC;;;C002\r\n");
    assert(p.cids[1] == 0x0dfc && durable_cids[1] == 0x0dfc);
    et100_serial_init(&p, "C00102030405", capture, NULL);
    et100_serial_set_storage(&p, 1, durable_gtime, save_setting, NULL);
    et100_serial_set_cid_storage(&p, durable_cids, save_cids);
    expect(&p, "BLEGW$GCID;BOOT\r\n", "AT$PD=BLEGW$GCID;1;0639;0DFC;;;BOOT\r\n");
    const char *bad_cids[] = {"639;;;", "ZZZZ;;;", "0639;;", "0639;;;;", "0639;0DFC;;ABCDE"};
    for (unsigned i = 0; i < sizeof(bad_cids) / sizeof(bad_cids[0]); i++) {
        char request[100];
        snprintf(request, sizeof(request), "BLEGW$SCID;%s;FAIL\r\n", bad_cids[i]);
        expect(&p, request, "AT$PD=BLEGW$SCID;0;;;;;FAIL\r\n");
        assert(p.cids[0] == 0x0639 && p.cids[1] == 0x0dfc);
    }
    save_fails = 1;
    expect(&p, "BLEGW$SCID;1234;;;;DISK\r\n", "AT$PD=BLEGW$SCID;0;1234;;;;DISK\r\n");
    assert(p.cids[0] == 0x0639 && durable_cids[0] == 0x0639);
    save_fails = 0;
    expect(&p, "BLEGW$SCID;;;;;NONE\r\n", "AT$PD=BLEGW$SCID;1;;;;;NONE\r\n");
    assert(p.cids[0] == 0 && p.cids[1] == 0);
    expect(&p, "BLEGW$SCID;0000;;;;ZERO\r\n", "AT$PD=BLEGW$SCID;1;;;;;ZERO\r\n");
    puts("PASS: CID defaults, fixed fields, normalization, empty filters, mock reload/failure");
    return 0;
}
