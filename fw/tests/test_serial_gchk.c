#include "serial_gchk.h"
#include "../include/eup_fw_info.h"
#include "smp_server.h"
#include "system_status.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char output[20000];
static size_t used;
static uint16_t durable_gtime = ET100_GTIME_DEFAULT;
static int save_fails;
static unsigned save_calls;
static uint16_t durable_cids[4] = {0x0639};
static uint8_t durable_controls[ET100_REPORT_CONTROLS_SIZE];
static unsigned controls_saves;
static int save_controls(const struct et100_report_controls *c, void *context)
{
    (void)context;
    controls_saves++;
    if (save_fails) return -1;
    et100_controls_encode(c, durable_controls);
    return 0;
}
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

static struct sensor_store_status pkg;
static int pkg_ok = 1;
static struct inst_snapshot ota;
static int ota_status(struct inst_snapshot *out) { *out=ota;return ota.ready; }
static int pkg_status(struct sensor_store_status *out)
{
    *out = pkg;
    return pkg_ok;
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
    output[0] = '\0';
    feed(p, request);
    if (strcmp(output, response)) fprintf(stderr, "got: %s\nwant: %s\n", output, response);
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
    assert(strstr(output, "AT$PD=BLEGW$GMAXRPT;0;3;Ab12\r\n") == output);
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
    expect(&p, "BLEGW$SGTIME;1;MIN1\r\n", "AT$PD=BLEGW$SGTIME;0;1;MIN1\r\n");
    p.save_controls = save_controls;
    expect(&p, "BLEGW$SMAXRPT;1;MIN2\r\n", "AT$PD=BLEGW$SMAXRPT;1;1;MIN2\r\n");
    expect(&p, "BLEGW$SGTIME;1;MIN1\r\n", "AT$PD=BLEGW$SGTIME;1;1;MIN1\r\n");
    expect(&p, "BLEGW$SGTIME;1800;MAX1\r\n", "AT$PD=BLEGW$SGTIME;1;1800;MAX1\r\n");
    save_fails = 1;
    expect(&p, "BLEGW$SGTIME;60;DISK\r\n", "AT$PD=BLEGW$SGTIME;0;60;DISK\r\n");
    expect(&p, "BLEGW$GGTIME;KEEP\r\n", "AT$PD=BLEGW$GGTIME;1;1800;KEEP\r\n");
    assert(durable_gtime == 1800);
    et100_serial_set_storage(&p, 1, 0, save_setting, NULL);
    assert(p.gtime == ET100_GTIME_DEFAULT);
    puts("PASS: GTIME default, set/query, reload, 1/1800 boundaries, invalid input, storage failures");
    expect(&p, "BLEGW$OTABAUD;115200;B000\r\n", "AT$PD=BLEGW$OTABAUD;0;115200;B000\r\n");
    assert(p.baud_request == 0);                       /* transport did not opt in */
    p.baud_supported = 1;
    expect(&p, "BLEGW$OTABAUD;115200;B001\r\n", "AT$PD=BLEGW$OTABAUD;1;115200;B001\r\n");
    assert(p.baud_request == 115200);
    p.baud_request = 0;
    expect(&p, "BLEGW$OTABAUD;9600;B002\r\n", "AT$PD=BLEGW$OTABAUD;1;9600;B002\r\n");
    assert(p.baud_request == 9600);
    p.baud_request = 0;
    expect(&p, "BLEGW$OTABAUD;921600;B003\r\n", "AT$PD=BLEGW$OTABAUD;0;921600;B003\r\n");
    expect(&p, "BLEGW$OTABAUD;B004\r\n", "AT$PD=BLEGW$OTABAUD;0;;B004\r\n");
    assert(p.baud_request == 0);
    puts("PASS: OTABAUD 9600/115200 accepted only when supported; other rates rejected");
    expect(&p, "BLEGW$GFWINFO;F000\r\n", "AT$PD=BLEGW$GFWINFO;0;;;;;F000\r\n");
    static const struct eup_fw_info fw = { .project_name = "EUP_ET100",
        .hardware_version = "ET100_EVB_DEV1.4", .firmware_version = "0.8.7-dev" };
    p.fw_info = &fw;
    expect(&p, "BLEGW$GFWINFO;F001\r\n",
           "AT$PD=BLEGW$GFWINFO;1;EUP_ET100;ET100_EVB_DEV1.4;0.8.7-dev;fr3068e_c;F001\r\n");
    static struct eup_fw_info wide;
    memset(wide.project_name, 'P', sizeof wide.project_name);   /* unterminated */
    p.fw_info = &wide;
    expect(&p, "BLEGW$GFWINFO;F002\r\n",
           "AT$PD=BLEGW$GFWINFO;1;PPPPPPPPPPPPPPPPPPPPPPPP;;;fr3068e_c;F002\r\n");
    p.fw_info = NULL;
    puts("PASS: GFWINFO from EUPFWV1, missing source, unterminated fields");
    expect(&p,"BLEGW$GOTASTAT;O000\r\n","AT$PD=BLEGW$GOTASTAT;0;O000\r\n");
    p.ota_status=ota_status;ota.ready=true;ota.state=BOOT_CONFIRMED;
    expect(&p,"BLEGW$GOTASTAT;O001\r\n","AT$PD=BLEGW$GOTASTAT;1;CONFIRMED;-;-;0;;O001\r\n");
    ota.has_record=true;ota.record.result=IR_ROLLED_BACK;ota.record.code=IC_TRIAL_TIMEOUT;
    ota.record.hash[0]=0xab;p.fw_info=&fw;
    expect(&p,"BLEGW$GOTASTAT;O002\r\n","AT$PD=BLEGW$GOTASTAT;1;CONFIRMED;AB000000;ROLLED_BACK;9;0.8.7-dev;O002\r\n");
    p.fw_info=NULL;
    /* v0.8.8 GSOTAPKG / DSOTAPKG: eup_gateway rs232_cmd_handler.c formats */
    expect(&p, "BLEGW$GSOTAPKG;P000\r\n", "AT$PD=BLEGW$GSOTAPKG;0;UNSUPPORTED;P000\r\n");
    expect(&p, "BLEGW$DSOTAPKG;P001\r\n", "AT$PD=BLEGW$DSOTAPKG;0;UNSUPPORTED;P001\r\n");
    p.pkg_status = pkg_status;
    expect(&p, "BLEGW$GSOTAPKG;P002\r\n", "AT$PD=BLEGW$GSOTAPKG;1;EMPTY;0;-;-;-;-;-;-;-;-;P002\r\n");
    pkg.state = SENSOR_STORE_INVALID; pkg.generation = 7;
    expect(&p, "BLEGW$GSOTAPKG;P003\r\n", "AT$PD=BLEGW$GSOTAPKG;1;INVALID;7;-;-;-;-;-;-;-;-;P003\r\n");
    pkg.state = SENSOR_STORE_VALID; pkg.generation = 8;
    pkg.info.package_length = 167312; pkg.info.image_length = 167056;
    strcpy(pkg.info.product, "EUP_AIoT_P1"); strcpy(pkg.info.board, "p1_nrf52832");
    strcpy(pkg.info.soc, "nrf52832"); strcpy(pkg.info.firmware, "v3.6.14");
    memset(pkg.info.firmware + 8, 0, sizeof pkg.info.firmware - 8);
    for (unsigned i = 0; i < 32; i++) pkg.info.upload_sha256[i] = (uint8_t)(0xd0 + i);
    expect(&p, "BLEGW$GSOTAPKG;P004\r\n",
           "AT$PD=BLEGW$GSOTAPKG;1;VALID;8;167312;167056;EUP_AIoT_P1;p1_nrf52832;nrf52832;v3.6.14;"
           "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDFE0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF;-;P004\r\n");
    strcpy(pkg.info.build_id, "20261017223216");
    memset(pkg.info.product, 'P', sizeof pkg.info.product);        /* unterminated: bounded */
    used = 0;
    feed(&p, "BLEGW$GSOTAPKG;P005\r\n");
    assert(strstr(output, ";VALID;8;167312;167056;PPPPPPPPPPPPPPPPPPPPPPPP;p1_nrf52832;") &&
           strstr(output, ";20261017223216;P005\r\n"));
    pkg_ok = 0;
    expect(&p, "BLEGW$GSOTAPKG;P006\r\n", "AT$PD=BLEGW$GSOTAPKG;0;READ_FAILED;P006\r\n");
    pkg_ok = 1;
    expect(&p, "BLEGW$DSOTAPKG;D001\r\n", "");                  /* answered after the erase */
    assert(p.pkg_delete_request && p.pkg_delete_busy);
    p.pkg_delete_request = 0;
    expect(&p, "BLEGW$GSOTAPKG;D002\r\n", "AT$PD=BLEGW$GSOTAPKG;0;BUSY;D002\r\n");
    expect(&p, "BLEGW$DSOTAPKG;D003\r\n", "AT$PD=BLEGW$DSOTAPKG;0;BUSY;D003\r\n");
    used = 0;
    et100_serial_pkg_deleted(&p, 0);
    assert(!strcmp(output, "AT$PD=BLEGW$DSOTAPKG;1;OK;D001\r\n") && !p.pkg_delete_busy);
    feed(&p, "BLEGW$DSOTAPKG;D004\r\n");
    used = 0;
    et100_serial_pkg_deleted(&p, SMP_EBUSY);
    assert(!strcmp(output, "AT$PD=BLEGW$DSOTAPKG;0;BUSY;D004\r\n"));
    feed(&p, "BLEGW$DSOTAPKG;D005\r\n");
    used = 0;
    et100_serial_pkg_deleted(&p, SMP_EUNKNOWN);
    assert(!strcmp(output, "AT$PD=BLEGW$DSOTAPKG;0;DELETE_FAILED;D005\r\n"));
    p.pkg_delete_request = 0;
    p.pkg_status = NULL;
    puts("PASS: GSOTAPKG EMPTY/INVALID/VALID/BUSY/READ_FAILED, DSOTAPKG deferred OK/BUSY/FAILED");
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

    p.save_controls = save_controls;
    expect(&p, "BLEGW$GMAXRPT;R001\r\n", "AT$PD=BLEGW$GMAXRPT;1;3;R001\r\n");
    expect(&p, "BLEGW$GLIST;R002\r\n", "AT$PD=BLEGW$GLIST;1;;;;;;;;;R002\r\n");
    expect(&p, "BLEGW$SLIST;aabbccddeeff;R003\r\n",
           "AT$PD=BLEGW$SLIST;1;AABBCCDDEEFF;;;;;;;;R003\r\n");
    assert(p.controls.macs[0][0] == 0xaa);
    struct et100_report_controls reloaded = {0};
    assert(et100_controls_decode(&reloaded, durable_controls, p.gtime));
    assert(!memcmp(&reloaded, &p.controls, sizeof reloaded));
    for (unsigned i = 0; i < sizeof durable_controls; i++) {
        durable_controls[i] ^= 1;
        assert(!et100_controls_decode(&reloaded, durable_controls, p.gtime));
        durable_controls[i] ^= 1;
    }
    unsigned saved = controls_saves;
    const char *bad_counts[] = {"0", "16", "1800", "256", "-1", "+1", "1;2", "", "999999999999999"};
    for (unsigned i = 0; i < sizeof bad_counts / sizeof bad_counts[0]; i++) {
        char req[100], rsp[120];
        snprintf(req, sizeof req, "BLEGW$SMAXRPT;%s;FAIL\r\n", bad_counts[i]);
        snprintf(rsp, sizeof rsp, "AT$PD=BLEGW$SMAXRPT;0;%s;FAIL\r\n", bad_counts[i]);
        expect(&p, req, rsp);
    }
    const char *bad_lists[] = {"AABBCCDDEEF", "AABBCCDDEEFG", ";;;;;;;;", "AABBCCDDEEFF;1"};
    for (unsigned i = 0; i < sizeof bad_lists / sizeof bad_lists[0]; i++) {
        char req[150];
        snprintf(req, sizeof req, "BLEGW$SLIST;%s;FAIL\r\n", bad_lists[i]);
        expect(&p, req, "AT$PD=BLEGW$SLIST;0;;;;;;;;;FAIL\r\n");
    }
    assert(controls_saves == saved && p.controls.maxrpt == 3 && p.controls.macs[0][0] == 0xaa);
    expect(&p, "BLEGW$SMAXRPT;15;R004\r\n", "AT$PD=BLEGW$SMAXRPT;1;15;R004\r\n");
    expect(&p, "BLEGW$SLIST;000000000001;000000000002;000000000003;000000000004;000000000005;000000000006;000000000007;000000000008;R005\r\n",
           "AT$PD=BLEGW$SLIST;1;000000000001;000000000002;000000000003;000000000004;000000000005;000000000006;000000000007;000000000008;R005\r\n");
    expect(&p, "BLEGW$SMAXRPT;7;R006\r\n", "AT$PD=BLEGW$SMAXRPT;0;7;R006\r\n");
    expect(&p, "BLEGW$SGTIME;14;R007\r\n", "AT$PD=BLEGW$SGTIME;0;14;R007\r\n");
    save_fails = 1;
    expect(&p, "BLEGW$SLIST;;FAIL\r\n", "AT$PD=BLEGW$SLIST;0;;;;;;;;;FAIL\r\n");
    expect(&p, "BLEGW$SMAXRPT;8;FAIL\r\n", "AT$PD=BLEGW$SMAXRPT;0;8;FAIL\r\n");
    assert(p.controls.maxrpt == 15 && p.controls.macs[7][5] == 8);
    save_fails = 0;
    expect(&p, "BLEGW$SLIST;;;;;;;;;R008\r\n", "AT$PD=BLEGW$SLIST;1;;;;;;;;;R008\r\n");
    expect(&p, "BLEGW$SMAXRPT;1;R009\r\n", "AT$PD=BLEGW$SMAXRPT;1;1;R009\r\n");
    expect(&p, "BLEGW$SGTIME;1;R010\r\n", "AT$PD=BLEGW$SGTIME;1;1;R010\r\n");
    expect(&p, "BLEGW$SMAXRPT;2;R011\r\n", "AT$PD=BLEGW$SMAXRPT;0;2;R011\r\n");
    et100_serial_init(&p, "C00102030405", capture, NULL);
    et100_serial_set_storage(&p, 1, durable_gtime, save_setting, NULL);
    assert(et100_controls_decode(&p.controls, durable_controls, p.gtime));
    expect(&p, "BLEGW$GMAXRPT;BOOT\r\n", "AT$PD=BLEGW$GMAXRPT;1;1;BOOT\r\n");
    p.storage_ready = 0;
    p.save_controls = save_controls;
    saved = controls_saves;
    expect(&p, "BLEGW$SMAXRPT;1;FAIL\r\n", "AT$PD=BLEGW$SMAXRPT;0;1;FAIL\r\n");
    assert(saved == controls_saves);
    puts("PASS: whitelist/MAXRPT commands, 8 slots, normalization, constraints, persistence codec corruption/reload/failure");
    et100_sys_init();p.system_status=et100_sys_query;
    char sys_expected[SYS_LINE_MAX+1];
    assert(et100_sys_query("A001",0,sys_expected,sizeof sys_expected));
    expect(&p,"BLEGW$GSYSSTAT;A001\r\n",sys_expected);
    assert(et100_sys_query("A002",1,sys_expected,sizeof sys_expected));
    expect(&p,"BLEGW$GSYSSTAT;extra;A002\r\n",sys_expected);
    expect(&p,"BLEGW$GSYSSTAT;abcd\r\n","");
    expect(&p,"BLEGW$GSYSSTAT;A!01\r\n","");
    p.system_status=NULL;
    assert(et100_sys_query("A003",2,sys_expected,sizeof sys_expected));
    expect(&p,"BLEGW$GSYSSTAT;A003\r\n",sys_expected);
    puts("PASS: GSYSSTAT full snapshot, CRC/token, malformed params, no hardware callback");
    return 0;
}
