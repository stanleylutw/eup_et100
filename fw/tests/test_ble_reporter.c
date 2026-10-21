#include "ble_reporter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct et100_reporter r;
static char out[128];
static const uint8_t mac[] = {0xff,0xee,0xdd,0xcc,0xbb,0xaa};
static const uint16_t defaults[] = {0x0639,0,0,0};
static const uint8_t sensor[] = {3,0xff,0x39,0x06};

static void check_checksum(void)
{
    unsigned checksum;
    char *star = strchr(out, '*');
    uint8_t actual = 0;
    assert(star && sscanf(star + 1, "%2x", &checksum) == 1);
    for (char *p = out + 6; p < star; p++) actual ^= (uint8_t)*p;
    assert(actual == checksum && strcmp(star + 3, "\r\n") == 0);
}

int main(void)
{
    et100_reporter_init(&r, defaults, 15, 0);
    const uint8_t bad[] = {4,0xff,0x39};
    const uint8_t other[] = {3,0xff,0xfc,0x0d};
    assert(!et100_reporter_receive(&r, mac, 0, -50, bad, sizeof(bad), 100));
    assert(!et100_reporter_receive(&r, mac, 0, -50, other, sizeof(other), 100));
    assert(et100_reporter_receive(&r, mac, 0, -50, sensor, sizeof(sensor), 100));
    assert(!et100_reporter_poll(&r, 14999, out, sizeof(out)));
    assert(et100_reporter_poll(&r, 15000, out, sizeof(out)));
    assert(strstr(out, "AT$PD=G1;1;AABBCCDDEEFF;3906;0050*") == out);
    check_checksum();
    assert(!et100_reporter_poll(&r, 30000, out, sizeof(out)));

    uint16_t ffff[] = {0xffff,0,0,0};
    const uint8_t vector[] = {11,0xff,0xff,0xff,0x01,0x0a,0xb8,0x0b,0x01,0xea,0x00,0x11};
    et100_reporter_init(&r, ffff, 15, 0);
    assert(et100_reporter_receive(&r, mac, 0, -50, vector, sizeof(vector), 0));
    assert(et100_reporter_poll(&r, 15000, out, sizeof(out)));
    assert(strcmp(out, "AT$PD=G1;1;AABBCCDDEEFF;FFFF010AB80B01EA0011;0050*3F\r\n") == 0);

    const uint8_t mixed[] = {3,0xff,0xfc,0x0d,3,0x16,0x39,0x06};
    et100_reporter_init(&r, defaults, 2, 0);
    assert(et100_reporter_receive(&r, mac, 0, 20, mixed, sizeof(mixed), 0));
    assert(et100_reporter_poll(&r, 2000, out, sizeof(out)));
    assert(strstr(out, ";3906;1020*"));
    check_checksum();

    const uint8_t named[] = {3,0xff,0x39,0x06,4,0x09,'A','B','C'};
    et100_reporter_init(&r, defaults, 2, 0);
    assert(et100_reporter_receive(&r, mac, 0, -1, named, sizeof(named), 0));
    assert(et100_reporter_poll(&r, 2000, out, sizeof(out)));
    assert(strstr(out, ";39060409414243;0001*"));

    uint16_t all[] = {0,0,0,0};
    et100_reporter_init(&r, all, 2, 0);
    assert(et100_reporter_receive(&r, mac, 0, -60, other, sizeof(other), 0));
    uint8_t second[6] = {1,2,3,4,5,6};
    assert(et100_reporter_receive(&r, second, 0, -20, sensor, sizeof(sensor), 0));
    assert(et100_reporter_poll(&r, 2000, out, sizeof(out)));
    assert(strstr(out, "060504030201"));
    assert(!et100_reporter_poll(&r, 2999, out, sizeof(out)));
    assert(et100_reporter_poll(&r, 3000, out, sizeof(out)));
    assert(strstr(out, "AABBCCDDEEFF"));
    assert(!et100_reporter_poll(&r, 4000, out, sizeof(out)));

    et100_reporter_init(&r, defaults, 15, 0);
    uint8_t eup[] = {17,0xff,0x39,0x06,0x10,1,2,3,0,0,0x15,0,0,0,0,0,0,0};
    assert(et100_reporter_receive(&r, mac, 0, -50, eup, sizeof(eup), 100));
    assert(et100_reporter_poll(&r, 15000, out, sizeof(out)));
    assert(et100_reporter_receive(&r, mac, 0, -45, eup, sizeof(eup), 15100));
    assert(!et100_reporter_poll(&r, 30000, out, sizeof(out)));
    eup[10] = 0x16;
    assert(et100_reporter_receive(&r, mac, 0, -40, eup, sizeof(eup), 30100));
    assert(et100_reporter_poll(&r, 45000, out, sizeof(out)));
    assert(et100_reporter_receive(&r, mac, 0, -40, eup, sizeof(eup), 46500));
    assert(et100_reporter_poll(&r, 60000, out, sizeof(out)));

    et100_reporter_init(&r, defaults, 1, UINT32_MAX - 999);
    assert(et100_reporter_receive(&r, mac, 0, -50, sensor, sizeof(sensor), UINT32_MAX - 500));
    assert(et100_reporter_poll(&r, 0, out, sizeof(out)));
    check_checksum();

    et100_reporter_init(&r, all, 15, 0);
    for (unsigned i = 0; i < 40; i++) {
        second[0] = (uint8_t)i;
        assert(et100_reporter_receive(&r, second, 0, -30, sensor, sizeof(sensor), i));
    }
    unsigned emitted = 0;
    for (unsigned i = 0; i < 15; i++)
        emitted += et100_reporter_poll(&r, 15000 + i * 1000, out, sizeof(out)) != 0;
    assert(emitted == 15);

    uint32_t seed = 1;
    for (unsigned sample = 0; sample < 5000; sample++) {
        uint8_t data[31];
        for (unsigned i = 0; i < 31; i++) {
            seed = seed * 1664525u + 1013904223u;
            data[i] = (uint8_t)(seed >> 24);
        }
        et100_reporter_receive(&r, mac, 0, -50, data, sample % 32, sample);
    }
    puts("PASS: CID/UUID filtering, malformed AD, MAC/RSSI/XOR vector, names, sorting/throttle,");
    puts("      fresh-only reports, EUP burst dedup, clock wrap, bounded cache, 5000 AD fuzz samples");
    return 0;
}
