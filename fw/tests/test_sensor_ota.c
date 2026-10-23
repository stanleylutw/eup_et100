/* v0.8.9 host tests: Sensor OTA relay job + SMP client + compat, driven against a
 * fake Sensor (MCUmgr image/OS groups + EUP metadata) behind a fake GATT link. */
#include <assert.h>
#include <stdio.h>
#include "../src/sensor_ota_job.c"
#include "cbor_min.h"
#include "smp_server.h"

#define IMG_LEN 5000u
static uint32_t ticks;
uint32_t xTaskGetTickCount(void) { return ticks; }
void vTaskDelay(uint32_t n) { ticks += n ? n : 1; }

/* ---- package (OTA worker fakes) ---- */
static uint8_t image[IMG_LEN];
static struct sensor_store_status pkg;
static int pkg_supported, relay_active, relay_refuse, reads;
bool ota_worker_sensor_status(struct sensor_store_status *out)
{ if (!pkg_supported) return false; *out = pkg; return true; }
bool ota_worker_relay_begin(void) { if (relay_refuse) return false; assert(!relay_active); relay_active = 1; return true; }
void ota_worker_relay_end(void) { assert(relay_active); relay_active = 0; }
int ota_worker_sensor_read(uint32_t off, void *data, size_t n)
{
    assert(relay_active && off >= SENSOR_PKG_HEADER_SIZE && off - SENSOR_PKG_HEADER_SIZE + n <= IMG_LEN);
    memcpy(data, image + (off - SENSOR_PKG_HEADER_SIZE), n); reads++; return 0;
}

/* ---- fake Sensor ---- */
static struct {
    int connect_fail, drain_stuck;
    int advertising, auth_fail, smp_missing, boot_fail, reset_silent, bad_seq, bad_state;
    unsigned drop_after_chunks, cancel_after_chunks, chunks, erases, resets;
    char project[25], fw[33];
    uint8_t active_hash[32], slot1[IMG_LEN], slot1_hash[32], upload_sha[32];
    uint32_t slot1_off, upload_len;
    int slot1_valid, pending;
} sensor;
static int link_up, gatt_busy, reserved_flag, cancel_flag, sessions;
static uint16_t fake_mtu = 247, notify_size;
static void (*sink)(const uint8_t *, size_t);
static int (*hook)(void);
static uint8_t req[1100], rsp[600];
static size_t req_len;
static char events[60000];
static size_t events_len;
static const uint8_t MAC[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

unsigned et100_gatt_state(void) { return (unsigned)gatt_busy; }
void et100_gatt_set_idle_hook(int (*h)(void)) { hook = h; }
int et100_gatt_reserve(void) { if (reserved_flag || gatt_busy) return -EBUSY; reserved_flag = 1; cancel_flag = 0; return 0; }
void et100_gatt_unreserve(void) { reserved_flag = 0; cancel_flag = 0; }
void et100_gatt_cancel(void) { if (reserved_flag) cancel_flag = 1; }
uint16_t et100_gatt_mtu(void) { return fake_mtu; }
void et100_gatt_op_event(const uint8_t mac[6], const char *data)
{
    assert(!memcmp(mac, MAC, 6));
    int n = snprintf(events + events_len, sizeof events - events_len, "%s\n", data);
    assert(n > 0 && events_len + (size_t)n < sizeof events);
    events_len += (size_t)n;
}
int et100_gatt_wait(volatile unsigned *flag, uint32_t ms)
{
    if (cancel_flag) return -ECANCELED;
    if (!link_up) return -ENOTCONN;
    if (*flag) return 0;
    ticks += ms;
    return -ETIMEDOUT;
}
int et100_gatt_metadata(const uint8_t mac[6], struct eup_gatt_metadata *m)
{
    memset(m, 0, sizeof *m);
    snprintf(m->project_name, sizeof m->project_name, "%s", sensor.project);
    snprintf(m->hardware_version, sizeof m->hardware_version, "P1_HW_1.0.0");
    snprintf(m->fw_version, sizeof m->fw_version, "%s", sensor.fw);
    snprintf(m->build_id, sizeof m->build_id, "20261017223216");
    snprintf(m->soc, sizeof m->soc, "nrf52832");
    snprintf(m->device_id, sizeof m->device_id, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    m->capabilities = EUP_AIOT_REQUIRED_CAPS;
    m->valid_mask = EUP_GATT_META_PROJECT | EUP_GATT_META_HARDWARE | EUP_GATT_META_FIRMWARE |
        EUP_GATT_META_BUILD_ID | EUP_GATT_META_SOC | EUP_GATT_META_SCHEMA | EUP_GATT_META_PROTOCOL |
        EUP_GATT_META_DEVICE_ID | EUP_GATT_META_CAPABILITIES | EUP_GATT_META_AIOT_ROLE;
    m->command_success_mask = EUP_GATT_CMD_SUCCESS(1) | EUP_GATT_CMD_SUCCESS(5) |
        EUP_GATT_CMD_SUCCESS(7) | EUP_GATT_CMD_SUCCESS(8);
    m->metadata_schema = 1; m->protocol_version = 1; m->seen = true;
    return 0;
}
int et100_gatt_smp_open(void (*s)(const uint8_t *, size_t))
{
    if (!link_up) return -ENOTCONN;
    if (sensor.smp_missing) return -ENOENT;
    sink = s; req_len = 0;
    return 0;
}

static void respond(const uint8_t *request, const struct cbor_writer *w)
{
    memcpy(rsp, request, 8);
    rsp[0] = (uint8_t)(request[0] + 1);
    rsp[2] = (uint8_t)(w->len >> 8); rsp[3] = (uint8_t)w->len;
    assert(8 + w->len <= sizeof rsp);
    memcpy(rsp + 8, w->buf, w->len);
    if (sensor.bad_seq) rsp[6]++;
    size_t total = 8 + w->len, part = notify_size ? notify_size : (size_t)fake_mtu - 3;
    for (size_t off = 0; off < total; off += part) sink(rsp + off, total - off < part ? total - off : part);
}
static void put_images(struct cbor_writer *w)
{
    cbor_put_map(w, 1);
    cbor_put_text(w, "images");
    cbor_put_array(w, sensor.slot1_valid ? 2 : 1);
    cbor_put_map(w, 5);
    cbor_put_text(w, "slot"); cbor_put_uint(w, 0);
    cbor_put_text(w, "version"); cbor_put_text(w, "1.0.0");
    cbor_put_text(w, "hash"); cbor_put_bytes(w, sensor.active_hash, 32);
    cbor_put_text(w, "active"); cbor_put_bool(w, true);
    cbor_put_text(w, "confirmed"); cbor_put_bool(w, true);
    if (sensor.slot1_valid) {
        cbor_put_map(w, 4);
        cbor_put_text(w, "slot"); cbor_put_uint(w, 1);
        cbor_put_text(w, "version"); cbor_put_text(w, "2.0.0");
        cbor_put_text(w, "hash"); cbor_put_bytes(w, sensor.slot1_hash, 32);
        cbor_put_text(w, "pending"); cbor_put_bool(w, sensor.pending);
    }
}
static void sensor_reset(void)
{
    link_up = 0; sensor.resets++;
    if (sensor.pending && !sensor.boot_fail) {
        memcpy(sensor.active_hash, sensor.slot1_hash, 32);
        snprintf(sensor.fw, sizeof sensor.fw, "%s", pkg.info.firmware);
    }
    sensor.pending = 0;
}
static void process(const uint8_t *r, size_t n)
{
    struct cbor_writer w;
    uint8_t out[512];
    const uint8_t *body = r + 8;
    size_t body_len = n - 8;
    uint16_t group = (uint16_t)(r[4] << 8 | r[5]);
    struct cbor_value v;
    cbor_writer_init(&w, out, sizeof out);
    assert(body_len == 0 || cbor_map_valid(body, body_len));
    if (group == SMP_GROUP_OS && r[7] == 5 && r[0] == SMP_OP_WRITE) {
        if (!sensor.reset_silent) { cbor_put_map(&w, 0); respond(r, &w); }
        sensor_reset();
        return;
    }
    assert(group == SMP_GROUP_IMG);
    if (r[7] == 0 && r[0] == SMP_OP_READ) {
        if (sensor.bad_state) cbor_put_map(&w, 0);
        else put_images(&w);
    } else if (r[7] == 0 && r[0] == SMP_OP_WRITE) {
        struct cbor_value hash, confirm;
        assert(!cbor_map_find(body, body_len, "hash", &hash) && hash.len == 32);
        assert(!cbor_map_find(body, body_len, "confirm", &confirm) && confirm.type == CBOR_BOOL && !confirm.u);
        if (sensor.slot1_valid && !memcmp(hash.ptr, sensor.slot1_hash, 32)) sensor.pending = 1;
        put_images(&w);
    } else if (r[7] == 5 && r[0] == SMP_OP_WRITE) {
        assert(!cbor_map_find(body, body_len, "slot", &v) && v.u == 1);
        sensor.erases++; sensor.slot1_valid = 0; sensor.slot1_off = 0; sensor.upload_len = 0;
        cbor_put_map(&w, 1); cbor_put_text(&w, "rc"); cbor_put_uint(&w, 0);
    } else if (r[7] == 1 && r[0] == SMP_OP_WRITE) {
        struct cbor_value off, data, len, sha, img;
        assert(!cbor_map_find(body, body_len, "image", &img) && img.u == 0);
        assert(!cbor_map_find(body, body_len, "off", &off) && !cbor_map_find(body, body_len, "data", &data));
        assert(data.len <= SENSOR_OTA_CHUNK_SIZE);
        if (++sensor.chunks == sensor.cancel_after_chunks) assert(sensor_ota_job_cancel() == 0);
        if (sensor.drop_after_chunks && sensor.chunks == sensor.drop_after_chunks) { link_up = 0; return; }
        if (off.u == 0) {
            assert(!cbor_map_find(body, body_len, "len", &len) && !cbor_map_find(body, body_len, "sha", &sha));
            if (!(sensor.slot1_off && len.u == sensor.upload_len && !memcmp(sha.ptr, sensor.upload_sha, 32))) {
                sensor.upload_len = (uint32_t)len.u; memcpy(sensor.upload_sha, sha.ptr, 32);
                sensor.slot1_off = 0; sensor.slot1_valid = 0;
            }
        } else {
            assert(cbor_map_find(body, body_len, "len", &len) == 1);
        }
        if (off.u == sensor.slot1_off) {
            memcpy(sensor.slot1 + off.u, data.ptr, data.len);
            sensor.slot1_off += (uint32_t)data.len;
            if (sensor.slot1_off == sensor.upload_len) {
                sensor.slot1_valid = 1;
                memcpy(sensor.slot1_hash, pkg.info.mcuboot_image_hash, 32);
                if (memcmp(sensor.slot1, image, IMG_LEN)) sensor.slot1_hash[0] ^= 0xff;
            }
        }
        cbor_put_map(&w, 2);
        cbor_put_text(&w, "rc"); cbor_put_uint(&w, 0);
        cbor_put_text(&w, "off"); cbor_put_uint(&w, sensor.slot1_off);
    } else {
        assert(0);
    }
    assert(!w.error);
    respond(r, &w);
}
int et100_gatt_smp_write(const uint8_t *data, size_t length)
{
    if (!link_up) return -ENOTCONN;
    assert(length <= (size_t)fake_mtu - 3 && req_len + length <= sizeof req);
    memcpy(req + req_len, data, length);
    req_len += length;
    if (req_len >= 8 && req_len == 8u + (size_t)(req[2] << 8 | req[3])) {
        size_t n = req_len;
        req_len = 0;
        process(req, n);
    }
    return 0;
}
int et100_gatt_exclusive(const uint8_t mac[6], const struct et100_gatt_exclusive_ops *ops)
{
    assert(reserved_flag && relay_active && !memcmp(mac, MAC, 6));
    sessions++;
    if (cancel_flag) return -ECANCELED;
    if (!sensor.advertising) { ticks += ops->wait_ms; return -ETIMEDOUT; }
    ticks += 100;
    ops->matched(ops->ctx);
    if (sensor.connect_fail) {
        ticks+=10000;
        if (sensor.drain_stuck) { ticks+=15000;gatt_busy=1; }
        return -ENOTCONN;
    }
    link_up = 1;
    ops->stage(ET100_GATT_CONNECTED, 0, ops->ctx);
    ops->stage(ET100_GATT_AUTHENTICATING, 0, ops->ctx);
    if (sensor.auth_fail) {
        ops->stage(ET100_GATT_AUTHENTICATED, -EACCES, ops->ctx);
        link_up = 0;
        return -EACCES;
    }
    ops->stage(ET100_GATT_AUTHENTICATED, 0, ops->ctx);
    ops->stage(ET100_GATT_METADATA_READING, 0, ops->ctx);
    ops->stage(ET100_GATT_METADATA_READY, 0, ops->ctx);
    int result = ops->run(ops->ctx);
    if (result && cancel_flag) result = -ECANCELED;
    link_up = 0; sink = NULL;
    return result;
}

/* ---- helpers ---- */
static char line[400];
static const char *uart(const char *cmd, const char *params)
{
    line[0] = 0;
    assert(sensor_ota_job_uart(cmd, params, "T001", line, sizeof line) > 0);
    return line;
}
static void reset(void)
{
    memset(&sensor, 0, sizeof sensor);
    sensor.advertising = 1;
    snprintf(sensor.project, sizeof sensor.project, "EUP_AIoT_P1");
    snprintf(sensor.fw, sizeof sensor.fw, "v3.6.14");
    memset(sensor.active_hash, 0xa0, 32);
    link_up = gatt_busy = reserved_flag = cancel_flag = sessions = reads = 0;
    relay_active = relay_refuse = 0;
    fake_mtu = 247; notify_size = 0;
    events_len = 0; events[0] = 0;
    pkg_supported = 1;
    memset(&pkg, 0, sizeof pkg);
    pkg.state = SENSOR_STORE_VALID; pkg.generation = 3;
    struct sensor_pkg_info *i = &pkg.info;
    i->image_length = IMG_LEN; i->package_length = IMG_LEN + SENSOR_PKG_HEADER_SIZE;
    snprintf(i->product, sizeof i->product, "EUP_AIoT_P1");
    snprintf(i->hardware, sizeof i->hardware, "P1_HW_1.0.0");
    snprintf(i->firmware, sizeof i->firmware, "v3.6.15");
    snprintf(i->soc, sizeof i->soc, "nrf52832");
    snprintf(i->board, sizeof i->board, "p1_nrf52832");
    snprintf(i->build_id, sizeof i->build_id, "20261017223216");
    memset(i->upload_sha256, 0x5a, 32);
    memset(i->mcuboot_image_hash, 0xb1, 32);
}
static void run(void)
{
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF"), "AT$PD=BLEGW$SENSOROTA;1;AABBCCDDEEFF;3;5000;T001\r\n"));
    assert(sensor_ota_job_busy() && reserved_flag && relay_active);
    assert(hook && hook() == 1 && hook() == 0);
    assert(!sensor_ota_job_busy() && !reserved_flag && !relay_active && !link_up);
}
static void expect(enum sensor_ota_job_state state, enum sensor_ota_job_error error)
{
    struct sensor_ota_job_status st;
    sensor_ota_job_get_status(&st);
    if (st.state != state || st.error != error) {
        printf("got %s/%s\n%s", sensor_ota_job_state_name(st.state), sensor_ota_job_error_name(st.error), events);
        assert(0);
    }
}
static enum sensor_ota_step_result step(enum sensor_ota_workflow_step s)
{
    struct sensor_ota_job_status st;
    sensor_ota_job_get_status(&st);
    return sensor_ota_workflow_result(st.step_results, s);
}

static void test_success(uint16_t mtu, uint16_t notify)
{
    reset(); fake_mtu = mtu; notify_size = notify;
    run();
    expect(SENSOR_OTA_JOB_SUCCESS, SENSOR_OTA_ERROR_NONE);
    assert(!memcmp(sensor.slot1, image, IMG_LEN) && sensor.erases == 1 && sensor.resets == 1);
    assert(!memcmp(sensor.active_hash, pkg.info.mcuboot_image_hash, 32) && !strcmp(sensor.fw, "v3.6.15"));
    /* 1 KB cache refilled at the chunk that crosses its end: one read per >= 832 B. */
    assert(sessions == 2 && reads > 0 && reads <= (int)((IMG_LEN + 831) / 832));
    for (enum sensor_ota_workflow_step s = SENSOR_OTA_STEP_PACKAGE_CHECK; s <= SENSOR_OTA_STEP_RESULT; s++)
        assert(step(s) == SENSOR_OTA_STEP_PASS);
    assert(strstr(events, "SOTA;ERASE_DONE;") && strstr(events, "SOTA;SCAN_PAUSED;1\n") &&
           strstr(events, "SOTA;LINK_MTU;") && strstr(events, "SOTA;UPLOADING;5000\n") &&
           strstr(events, "SOTA;WAIT_REBOOT;5000\n") && strstr(events, "SOTA;SUCCESS;0\n") &&
           strstr(events, "SOTA;STEP;RESULT;PASS\n") && strstr(events, "SOTA;SCAN_PAUSED;0\n"));
    const char *g = uart("GSENSOROTA", "");
    assert(!strncmp(g, "AT$PD=BLEGW$GSENSOROTA;1;SUCCESS;AABBCCDDEEFF;3;5000;5000;", 58) &&
           strstr(g, ";0;NONE;T001\r\n"));
    uint64_t all_pass = 0;
    for (unsigned s = 0; s < 15; s++) all_pass |= (uint64_t)SENSOR_OTA_STEP_PASS << (3 * s);
    char expected[160];
    assert(all_pass == 0x092492492492ull);
    snprintf(expected, sizeof expected, "AT$PD=BLEGW$GSENSOROTAFLOW;1;SUCCESS;RESULT;PASS;NONE;000003FF;092492492492;T001\r\n");
    assert(!strcmp(uart("GSENSOROTAFLOW", ""), expected));
    snprintf(expected, sizeof expected, "AT$PD=BLEGW$GSENSOROTAMETA;1;AABBCCDDEEFF;EUP_AIoT_P1;P1_HW_1.0.0;v3.6.15;"
             "20261017223216;nrf52832;p1_nrf52832;AABBCCDDEEFF;%08lX;00;NONE;T001\r\n", (unsigned long)EUP_AIOT_REQUIRED_CAPS);
    assert(!strcmp(uart("GSENSOROTAMETA", ""), expected));
    assert(!strcmp(uart("CSENSOROTA", ""), "AT$PD=BLEGW$CSENSOROTA;0;NOT_ACTIVE;T001\r\n"));
}

static void test_up_to_date(void)
{
    reset();
    snprintf(sensor.fw, sizeof sensor.fw, "v3.6.15");
    memcpy(sensor.active_hash, pkg.info.mcuboot_image_hash, 32);
    run();
    expect(SENSOR_OTA_JOB_SUCCESS, SENSOR_OTA_ERROR_NONE);
    assert(sensor.erases == 0 && sensor.chunks == 0 && sensor.resets == 0 && sessions == 1 && reads == 0);
    for (enum sensor_ota_workflow_step s = SENSOR_OTA_STEP_ERASING; s <= SENSOR_OTA_STEP_RECONNECTING; s++)
        assert(step(s) == SENSOR_OTA_STEP_SKIPPED);
    assert(step(SENSOR_OTA_STEP_VERIFYING) == SENSOR_OTA_STEP_PASS && strstr(events, "SOTA;ALREADY_UP_TO_DATE;0\n"));
    assert(strstr(uart("GSENSOROTA", ""), ";0;ALREADY_UP_TO_DATE;T001\r\n"));
}

static void test_resume_after_drop(void)
{
    reset(); sensor.drop_after_chunks = 7;
    run();
    expect(SENSOR_OTA_JOB_SUCCESS, SENSOR_OTA_ERROR_NONE);
    /* Second session continues on the Sensor's offset without a second erase. */
    assert(sensor.erases == 1 && sessions == 3 && !memcmp(sensor.slot1, image, IMG_LEN));
    assert(sensor.chunks < 2 * ((IMG_LEN + SENSOR_OTA_CHUNK_SIZE - 1) / SENSOR_OTA_CHUNK_SIZE));
}

static void test_failures(void)
{
    reset();sensor.connect_fail=1;sensor.drain_stuck=1;
    run();expect(SENSOR_OTA_JOB_FAILED,SENSOR_OTA_ERROR_CONNECT_FAILED);
    assert(sessions==1 && gatt_busy && !relay_active);
    assert(step(SENSOR_OTA_STEP_CONNECTING)==SENSOR_OTA_STEP_FAIL);
    assert(strstr(uart("SENSOROTA","AABBCCDDEEFF"),";0;4;BUSY;"));

    reset();sensor.connect_fail=1;
    run();expect(SENSOR_OTA_JOB_FAILED,SENSOR_OTA_ERROR_CONNECT_FAILED);
    assert(sessions==SENSOR_OTA_MAX_SESSIONS && !gatt_busy);

    reset(); sensor.bad_state = 1;
    run();
    expect(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_LINK_BUDGET);
    assert(sensor.erases == 0 && sensor.chunks == 0 && sensor.resets == 0);

    reset(); snprintf(sensor.project, sizeof sensor.project, "EUP_AIoT_E9");
    run();
    expect(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_TARGET_MISMATCH);
    assert(sensor.erases == 0 && sessions == 1 && step(SENSOR_OTA_STEP_CHECKING_TARGET) == SENSOR_OTA_STEP_FAIL);
    assert(strstr(uart("GSENSOROTAMETA", ""), ";PROJECT;T001\r\n"));
    assert(strstr(uart("GSENSOROTAFLOW", ""), "AT$PD=BLEGW$GSENSOROTAFLOW;1;FAILED;RESULT;FAIL;CHECKING_TARGET;00000001;"));

    reset(); sensor.advertising = 0;
    run();
    expect(SENSOR_OTA_JOB_TIMEOUT, SENSOR_OTA_ERROR_WAIT_ADV_TIMEOUT);
    assert(sessions == 2);

    reset(); sensor.cancel_after_chunks = 4;
    run();
    expect(SENSOR_OTA_JOB_CANCELLED, SENSOR_OTA_ERROR_CANCELLED);
    assert(sensor.resets == 0 && strstr(events, "SOTA;CANCELLED;19\n"));

    reset(); sensor.boot_fail = 1;
    run();
    expect(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_VERSION_MISMATCH);
    assert(sensor.resets == 1 && step(SENSOR_OTA_STEP_VERIFYING) == SENSOR_OTA_STEP_FAIL);

    reset(); sensor.smp_missing = 1;
    run();
    expect(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_SMP_NOT_FOUND);
    assert(sessions == 1);

    reset(); sensor.auth_fail = 1;
    run();
    expect(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_AUTH_FAILED);
    assert(sessions == 1 && step(SENSOR_OTA_STEP_AUTHENTICATING) == SENSOR_OTA_STEP_FAIL);

    reset(); sensor.reset_silent = 1;          /* Sensor reboots before answering reset */
    run();
    expect(SENSOR_OTA_JOB_SUCCESS, SENSOR_OTA_ERROR_NONE);

    reset();                                   /* package replaced after the start ACK */
    assert(strstr(uart("SENSOROTA", "AABBCCDDEEFF"), ";1;"));
    pkg.generation++;
    assert(hook());
    expect(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_PACKAGE_INVALID);
    assert(sensor.erases == 0 && !relay_active && !reserved_flag);
}

static void test_start_errors(void)
{
    reset();
    assert(!strcmp(uart("GSENSOROTAMETA", ""), "AT$PD=BLEGW$GSENSOROTAMETA;0;NOT_READY;T001\r\n"));
    assert(!strcmp(uart("GSENSOROTA", ""), "AT$PD=BLEGW$GSENSOROTA;1;IDLE;000000000000;0;0;0;0;0;NONE;T001\r\n"));
    assert(!strcmp(uart("SENSOROTA", "AABBCC"), "AT$PD=BLEGW$SENSOROTA;0;5;BAD_MAC;T001\r\n"));
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF;1"), "AT$PD=BLEGW$SENSOROTA;0;5;BAD_MAC;T001\r\n"));
    assert(!strcmp(uart("SENSOROTA", "000000000000"), "AT$PD=BLEGW$SENSOROTA;0;5;BAD_MAC;T001\r\n"));
    gatt_busy = 1;
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF"), "AT$PD=BLEGW$SENSOROTA;0;4;BUSY;T001\r\n"));
    gatt_busy = 0; pkg.state = SENSOR_STORE_EMPTY;
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF"), "AT$PD=BLEGW$SENSOROTA;0;2;NO_PACKAGE;T001\r\n"));
    pkg.state = SENSOR_STORE_INVALID;
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF"), "AT$PD=BLEGW$SENSOROTA;0;3;PACKAGE_INVALID;T001\r\n"));
    pkg.state = SENSOR_STORE_VALID; pkg_supported = 0;
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF"), "AT$PD=BLEGW$SENSOROTA;0;1;UNSUPPORTED;T001\r\n"));
    pkg_supported = 1; snprintf(pkg.info.board, sizeof pkg.info.board, "x_nrf52840");
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF"), "AT$PD=BLEGW$SENSOROTA;0;9;TARGET_MISMATCH;T001\r\n"));
    snprintf(pkg.info.board, sizeof pkg.info.board, "p1_nrf52832"); relay_refuse = 1;
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF"), "AT$PD=BLEGW$SENSOROTA;0;4;BUSY;T001\r\n"));
    assert(!reserved_flag);
    relay_refuse = 0;
    assert(strstr(uart("SENSOROTA", "aabbccddeeff"), "SENSOROTA;1;aabbccddeeff;"));
    assert(!strcmp(uart("SENSOROTA", "AABBCCDDEEFF"), "AT$PD=BLEGW$SENSOROTA;0;4;BUSY;T001\r\n"));
    assert(!strcmp(uart("CSENSOROTA", ""), "AT$PD=BLEGW$CSENSOROTA;1;OK;T001\r\n"));
    assert(hook());
    expect(SENSOR_OTA_JOB_CANCELLED, SENSOR_OTA_ERROR_CANCELLED);
    assert(sessions == 0 && !sensor.chunks);   /* cancelled before the first session */
    char out[64];
    assert(sensor_ota_job_uart("GSENSOROTA", "X", "T001", out, sizeof out) == 0);
    assert(sensor_ota_job_uart("GCHK", "", "T001", out, sizeof out) == 0);
}

static void test_smp_client(void)
{
    static const struct sensor_smp_link fake = { et100_gatt_smp_write, et100_gatt_mtu, et100_gatt_wait, NULL, now_ms };
    struct sensor_smp_image_state images[3];
    size_t count;
    reset(); link_up = 1; sink = sensor_smp_client_rx;
    sensor_smp_client_begin(&fake);
    assert(sensor_smp_client_image_state(images, 3, &count) == 0 && count == 1 && images[0].active &&
           images[0].confirmed && !images[0].pending && images[0].slot == 0);
    sensor.bad_seq = 1;
    assert(sensor_smp_client_image_state(images, 3, &count) == -EPROTO);
    sensor.bad_seq = 0; notify_size = 7;
    assert(sensor_smp_client_image_state(images, 3, &count) == 0 && count == 1);
    assert(sensor_smp_client_image_test(pkg.info.mcuboot_image_hash) == -EBADMSG);   /* nothing in slot 1 */
    link_up = 0;
    assert(sensor_smp_client_image_state(images, 3, &count) == -ENOTCONN);
    sensor_smp_client_end();
}

static void test_flow_hex(void)
{
    static const struct { uint64_t bits; const char *hex; } cases[] = {
        {0, "000000000000"}, {1, "000000000001"}, {0xffffffffull, "0000FFFFFFFF"},
        {0x100000000ull, "000100000000"}, {0xabcd12345678ull, "ABCD12345678"},
        {UINT64_MAX, "FFFFFFFFFFFF"}
    };
    for (unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
        memset(&job_status,0,sizeof job_status);job_status.step_results=cases[i].bits;
        char expected[160],out[160];
        int n=snprintf(expected,sizeof expected,"AT$PD=BLEGW$GSENSOROTAFLOW;1;IDLE;NONE;PENDING;NONE;00000000;%s;Z9Q1\r\n",cases[i].hex);
        assert(sensor_ota_job_uart("GSENSOROTAFLOW","","Z9Q1",out,sizeof out)==n);
        assert(!strcmp(out,expected));
        for (size_t cap=1;cap<=(size_t)n;cap++) {
            unsigned char guarded[162];memset(guarded,0xa5,sizeof guarded);
            assert(sensor_ota_job_uart("GSENSOROTAFLOW","","Z9Q1",(char *)guarded+1,cap)==0);
            assert(guarded[0]==0xa5 && guarded[cap+1]==0xa5 && guarded[cap]==0);
        }
    }
    memset(&job_status,0,sizeof job_status);
    puts("PASS FLOW: 12 uppercase hex digits, high 16/low 32, token, bounded truncation");
}

int main(void)
{
    for (uint32_t i = 0; i < IMG_LEN; i++) image[i] = (uint8_t)(i * 31 + 7);
    sensor_ota_job_init();
    assert(hook == sensor_ota_job_poll);
    test_flow_hex();
    test_smp_client();
    test_start_errors();
    test_success(247, 0);
    test_success(23, 20);                      /* fragmented requests and notifications */
    test_up_to_date();
    test_resume_after_drop();
    test_failures();
    puts("PASS: sensor OTA relay: full update, MTU 23 fragmentation, already up to date, resume after link drop,");
    puts("      target/auth/SMP/version failures, adv timeout, cancel, package replaced, start errors, wire formats");
    return 0;
}
