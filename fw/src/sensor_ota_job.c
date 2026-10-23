#include "sensor_ota_job.h"
#include "sensor_smp_client.h"
#include "sensor_store.h"
#include "gatt_client.h"
#include "ota_worker.h"
#include "FreeRTOS.h"
#include "task.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* Status shared with the serial task (queries) under a critical section. */
static struct sensor_ota_job_status job_status;
static struct sensor_store_status job_package;
static volatile unsigned start_pending, cancel_requested, finalizing;
static uint32_t job_started_ms, last_progress_ms;
static bool reset_sent, upload_completed, image_erased;
static enum sensor_ota_job_error upload_error_hint, verify_error_hint;
static struct et100_gatt_exclusive_ops upload_ops, verify_ops;
/* Package image cache filled by the OTA worker (it owns the NOR). */
static uint8_t cache[1024];
static uint32_t cache_base, cache_len;

static uint32_t now_ms(void) { return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS; }
static void lock(void) { taskENTER_CRITICAL(); }
static void unlock(void) { taskEXIT_CRITICAL(); }

static bool state_active(enum sensor_ota_job_state state)
{
    return state >= SENSOR_OTA_JOB_WAIT_ADV && state <= SENSOR_OTA_JOB_VERIFYING;
}

const char *sensor_ota_job_state_name(enum sensor_ota_job_state state)
{
    switch (state) {
    case SENSOR_OTA_JOB_WAIT_ADV: return "WAIT_ADV";
    case SENSOR_OTA_JOB_CONNECTING: return "CONNECTING";
    case SENSOR_OTA_JOB_ERASING: return "ERASING";
    case SENSOR_OTA_JOB_UPLOADING: return "UPLOADING";
    case SENSOR_OTA_JOB_TESTING_IMAGE: return "TESTING_IMAGE";
    case SENSOR_OTA_JOB_RESETTING_TARGET: return "RESETTING_TARGET";
    case SENSOR_OTA_JOB_WAIT_REBOOT: return "WAIT_REBOOT";
    case SENSOR_OTA_JOB_VERIFYING: return "VERIFYING";
    case SENSOR_OTA_JOB_SUCCESS: return "SUCCESS";
    case SENSOR_OTA_JOB_FAILED: return "FAILED";
    case SENSOR_OTA_JOB_CANCELLED: return "CANCELLED";
    case SENSOR_OTA_JOB_TIMEOUT: return "TIMEOUT";
    default: return "IDLE";
    }
}

const char *sensor_ota_job_error_name(enum sensor_ota_job_error error)
{
    switch (error) {
    case SENSOR_OTA_ERROR_UNSUPPORTED: return "UNSUPPORTED";
    case SENSOR_OTA_ERROR_NO_PACKAGE: return "NO_PACKAGE";
    case SENSOR_OTA_ERROR_PACKAGE_INVALID: return "PACKAGE_INVALID";
    case SENSOR_OTA_ERROR_BUSY: return "BUSY";
    case SENSOR_OTA_ERROR_BAD_MAC: return "BAD_MAC";
    case SENSOR_OTA_ERROR_WAIT_ADV_TIMEOUT: return "WAIT_ADV_TIMEOUT";
    case SENSOR_OTA_ERROR_CONNECT_FAILED: return "CONNECT_FAILED";
    case SENSOR_OTA_ERROR_AUTH_FAILED: return "AUTH_FAILED";
    case SENSOR_OTA_ERROR_TARGET_MISMATCH: return "TARGET_MISMATCH";
    case SENSOR_OTA_ERROR_SMP_NOT_FOUND: return "SMP_NOT_FOUND";
    case SENSOR_OTA_ERROR_LINK_BUDGET: return "LINK_BUDGET";
    case SENSOR_OTA_ERROR_UPLOAD_TIMEOUT: return "UPLOAD_TIMEOUT";
    case SENSOR_OTA_ERROR_OFFSET_INVALID: return "OFFSET_INVALID";
    case SENSOR_OTA_ERROR_IMAGE_TEST_FAILED: return "IMAGE_TEST_FAILED";
    case SENSOR_OTA_ERROR_RESET_FAILED: return "RESET_FAILED";
    case SENSOR_OTA_ERROR_REBOOT_TIMEOUT: return "REBOOT_TIMEOUT";
    case SENSOR_OTA_ERROR_VERSION_MISMATCH: return "VERSION_MISMATCH";
    case SENSOR_OTA_ERROR_HASH_MISMATCH: return "HASH_MISMATCH";
    case SENSOR_OTA_ERROR_CANCELLED: return "CANCELLED";
    case SENSOR_OTA_ERROR_INTERNAL: return "INTERNAL";
    case SENSOR_OTA_ERROR_IMAGE_ERASE_FAILED: return "IMAGE_ERASE_FAILED";
    case SENSOR_OTA_ERROR_METADATA_READ_FAILED: return "METADATA_READ_FAILED";
    case SENSOR_OTA_ERROR_CAPABILITY_MISMATCH: return "CAPABILITY_MISMATCH";
    default: return "NONE";
    }
}

const char *sensor_ota_workflow_step_name(enum sensor_ota_workflow_step step)
{
    static const char *const names[] = { "NONE", "PACKAGE_CHECK", "WAIT_ADV", "CONNECTING",
        "CONNECTED", "AUTHENTICATING", "CHECKING_TARGET", "PREPARING_LINK", "ERASING", "UPLOADING",
        "TESTING_IMAGE", "RESETTING_TARGET", "WAIT_REBOOT", "RECONNECTING", "VERIFYING", "RESULT" };
    return (unsigned)step < sizeof names / sizeof names[0] ? names[step] : "NONE";
}

const char *sensor_ota_step_result_name(enum sensor_ota_step_result result)
{
    switch (result) {
    case SENSOR_OTA_STEP_ACTIVE: return "ACTIVE";
    case SENSOR_OTA_STEP_PASS: return "PASS";
    case SENSOR_OTA_STEP_FAIL: return "FAIL";
    case SENSOR_OTA_STEP_SKIPPED: return "SKIPPED";
    default: return "PENDING";
    }
}

/* ---- Type 8 OP events (gateway rs232_reporter_send_gatt_event("OP", ...)) ---- */
static void op_event(const char *data)
{
    uint8_t mac[6];
    lock(); memcpy(mac, job_status.target_mac, 6); unlock();
    et100_gatt_op_event(mac, data);
}

static void workflow_set(enum sensor_ota_workflow_step step, enum sensor_ota_step_result result)
{
    char data[64];
    lock();
    job_status.workflow_step = step;
    job_status.step_result = result;
    sensor_ota_workflow_record(&job_status.step_results, &job_status.failed_step, step, result);
    unlock();
    snprintf(data, sizeof data, "SOTA;STEP;%s;%s", sensor_ota_workflow_step_name(step),
             sensor_ota_step_result_name(result));
    op_event(data);
}

static void workflow_skip_transfer(void)
{
    for (enum sensor_ota_workflow_step step = SENSOR_OTA_STEP_ERASING;
         step <= SENSOR_OTA_STEP_RECONNECTING; step++)
        workflow_set(step, SENSOR_OTA_STEP_SKIPPED);
}

static void report_state(enum sensor_ota_job_state state, uint32_t value)
{
    char data[56];
    lock();
    job_status.state = state;
    job_status.elapsed_ms = now_ms() - job_started_ms;
    unlock();
    snprintf(data, sizeof data, "SOTA;%s;%u", sensor_ota_job_state_name(state), (unsigned)value);
    op_event(data);
}

static void report_stage_error(const char *stage, int err)
{
    char data[56];
    snprintf(data, sizeof data, "SOTA;ERROR;%s;%d", stage, err);
    op_event(data);
}

static void report_link_value(const char *name, uint32_t value)
{
    char data[56];
    snprintf(data, sizeof data, "SOTA;%s;%u", name, (unsigned)value);
    op_event(data);
}

/* ---- SMP link over the GATT client ---- */
static int read_image(uint32_t offset, void *data, size_t length)
{
    uint32_t image_length = job_package.info.image_length;
    if (length > sizeof cache || offset > image_length || length > image_length - offset) return -1;
    if (!cache_len || offset < cache_base || offset + length > cache_base + cache_len) {
        uint32_t n = image_length - offset < sizeof cache ? image_length - offset : (uint32_t)sizeof cache;
        if (ota_worker_sensor_read(SENSOR_PKG_HEADER_SIZE + offset, cache, n)) {
            cache_len = 0;
            return -1;
        }
        cache_base = offset;
        cache_len = n;
    }
    memcpy(data, cache + (offset - cache_base), length);
    return 0;
}

static const struct sensor_smp_link smp_link = {
    et100_gatt_smp_write, et100_gatt_mtu, et100_gatt_wait, read_image, now_ms,
};

static int smp_open(void)
{
    int err = et100_gatt_smp_open(sensor_smp_client_rx);
    if (!err) sensor_smp_client_begin(&smp_link);
    return err;
}

/* ---- Session callbacks (GATT worker context) ---- */
static void progress_cb(uint32_t bytes_done, void *ctx)
{
    (void)ctx;
    lock();
    job_status.bytes_done = bytes_done;
    job_status.elapsed_ms = now_ms() - job_started_ms;
    unlock();
    if ((uint32_t)(now_ms() - last_progress_ms) >= 2000 || bytes_done == job_package.info.image_length) {
        last_progress_ms = now_ms();
        report_state(SENSOR_OTA_JOB_UPLOADING, bytes_done);
    }
}

static void erase_cb(int result, uint32_t elapsed_ms, void *ctx)
{
    char data[56];
    (void)ctx;
    if (result) {
        report_stage_error("IMAGE_ERASE", result);
        workflow_set(SENSOR_OTA_STEP_ERASING, SENSOR_OTA_STEP_FAIL);
        return;
    }
    image_erased = true;
    upload_error_hint = SENSOR_OTA_ERROR_UPLOAD_TIMEOUT;
    snprintf(data, sizeof data, "SOTA;ERASE_DONE;%u", (unsigned)elapsed_ms);
    op_event(data);
    workflow_set(SENSOR_OTA_STEP_ERASING, SENSOR_OTA_STEP_PASS);
    workflow_set(SENSOR_OTA_STEP_UPLOADING, SENSOR_OTA_STEP_ACTIVE);
    report_state(SENSOR_OTA_JOB_UPLOADING, job_status.bytes_done);
}

static bool image_hash_present(const struct sensor_smp_image_state *images, size_t count,
                               const uint8_t hash[32], bool active)
{
    for (size_t i = 0; i < count; i++)
        if (images[i].active == active && !memcmp(images[i].hash, hash, 32)) return true;
    return false;
}

static bool package_unchanged(void)
{
    struct sensor_store_status current;
    return ota_worker_sensor_status(&current) && current.state == SENSOR_STORE_VALID &&
           current.generation == job_package.generation &&
           current.info.image_length == job_package.info.image_length &&
           !memcmp(current.info.upload_sha256, job_package.info.upload_sha256, 32);
}

static int compare_target_metadata(enum sensor_ota_mismatch_field *field)
{
    struct sensor_ota_target_metadata snapshot = {0};
    struct sensor_ota_compat_result result;
    uint8_t mac[6];
    int err;
    lock(); memcpy(mac, job_status.target_mac, 6); unlock();
    *field = SENSOR_OTA_MISMATCH_NONE;
    if (et100_gatt_metadata(mac, &snapshot.sensor)) {
        err = -ENODATA;
    } else {
        snapshot.valid = true;
        err = sensor_ota_compat_evaluate(&job_package.info, &snapshot.sensor, mac, &result);
        snapshot.match_mask = result.match_mask;
        snprintf(snapshot.board, sizeof snapshot.board, "%s", result.board);
        *field = result.mismatch_field;
    }
    snapshot.mismatch_field = *field;
    lock(); job_status.target = snapshot; unlock();
    return err;
}

/* Running firmware equals the package: version and active MCUboot hash. */
static int verify_target_firmware(void)
{
    struct sensor_smp_image_state images[3];
    struct eup_gatt_metadata metadata;
    size_t count = 0;
    uint8_t mac[6];
    lock(); memcpy(mac, job_status.target_mac, 6); unlock();
    int err = sensor_smp_client_image_state(images, 3, &count);
    /* A malformed state is not evidence of an outdated image; never erase on it. */
    if (err == -EBADMSG) return -EPROTO;
    if (!err) err = et100_gatt_metadata(mac, &metadata);
    if (!err && strcmp(metadata.fw_version, job_package.info.firmware)) err = -EILSEQ;
    if (!err && !image_hash_present(images, count, job_package.info.mcuboot_image_hash, true)) err = -EBADMSG;
    if (!err) {
        lock();
        job_status.target.match_mask |= SENSOR_OTA_MATCH_FIRMWARE | SENSOR_OTA_MATCH_ACTIVE_HASH;
        unlock();
    }
    return err;
}

static void upload_matched(void *ctx)
{
    (void)ctx;
    upload_error_hint = SENSOR_OTA_ERROR_CONNECT_FAILED;
    workflow_set(SENSOR_OTA_STEP_WAIT_ADV, SENSOR_OTA_STEP_PASS);
    workflow_set(SENSOR_OTA_STEP_CONNECTING, SENSOR_OTA_STEP_ACTIVE);
    report_state(SENSOR_OTA_JOB_CONNECTING, 0);
}

static void verify_matched(void *ctx)
{
    (void)ctx;
    verify_error_hint = SENSOR_OTA_ERROR_CONNECT_FAILED;
    workflow_set(SENSOR_OTA_STEP_WAIT_REBOOT, SENSOR_OTA_STEP_PASS);
    workflow_set(SENSOR_OTA_STEP_RECONNECTING, SENSOR_OTA_STEP_ACTIVE);
}

/* gateway sensor_ota_job_stage_transition() */
static void stage_cb(enum et100_gatt_stage stage, int result, void *ctx)
{
    bool verifying = ctx == &verify_ops;
    if (verifying) {
        if (stage == ET100_GATT_CONNECTED) {
            workflow_set(SENSOR_OTA_STEP_RECONNECTING, SENSOR_OTA_STEP_PASS);
            workflow_set(SENSOR_OTA_STEP_VERIFYING, SENSOR_OTA_STEP_ACTIVE);
        } else if (stage == ET100_GATT_METADATA_READY && result) {
            workflow_set(SENSOR_OTA_STEP_VERIFYING, SENSOR_OTA_STEP_FAIL);
        }
        return;
    }
    switch (stage) {
    case ET100_GATT_CONNECTED:
        workflow_set(SENSOR_OTA_STEP_CONNECTING, SENSOR_OTA_STEP_PASS);
        workflow_set(SENSOR_OTA_STEP_CONNECTED, SENSOR_OTA_STEP_PASS);
        break;
    case ET100_GATT_AUTHENTICATING:
        workflow_set(SENSOR_OTA_STEP_AUTHENTICATING, SENSOR_OTA_STEP_ACTIVE);
        break;
    case ET100_GATT_AUTHENTICATED:
        if (result) upload_error_hint = SENSOR_OTA_ERROR_AUTH_FAILED;
        workflow_set(SENSOR_OTA_STEP_AUTHENTICATING, result ? SENSOR_OTA_STEP_FAIL : SENSOR_OTA_STEP_PASS);
        break;
    case ET100_GATT_METADATA_READING:
        workflow_set(SENSOR_OTA_STEP_CHECKING_TARGET, SENSOR_OTA_STEP_ACTIVE);
        break;
    case ET100_GATT_METADATA_READY:
        if (result) {
            upload_error_hint = SENSOR_OTA_ERROR_METADATA_READ_FAILED;
            workflow_set(SENSOR_OTA_STEP_CHECKING_TARGET, SENSOR_OTA_STEP_FAIL);
        }
        break;
    }
}

static int upload_run(void *ctx)
{
    struct sensor_smp_image_state images[3];
    size_t count = 0;
    uint32_t bytes_done = 0;
    enum sensor_ota_mismatch_field mismatch;
    (void)ctx;
    if (!package_unchanged()) {
        upload_error_hint = SENSOR_OTA_ERROR_PACKAGE_INVALID;
        return -ESTALE;
    }
    int err = compare_target_metadata(&mismatch);
    if (err) {
        upload_error_hint = err == -ENOTSUP ? SENSOR_OTA_ERROR_CAPABILITY_MISMATCH :
                            err == -ENODATA ? SENSOR_OTA_ERROR_METADATA_READ_FAILED :
                                              SENSOR_OTA_ERROR_TARGET_MISMATCH;
        workflow_set(SENSOR_OTA_STEP_CHECKING_TARGET, SENSOR_OTA_STEP_FAIL);
        return err;
    }
    workflow_set(SENSOR_OTA_STEP_CHECKING_TARGET, SENSOR_OTA_STEP_PASS);
    workflow_set(SENSOR_OTA_STEP_PREPARING_LINK, SENSOR_OTA_STEP_ACTIVE);
    err = smp_open();
    if (!err) err = verify_target_firmware();
    if (!err) {
        lock();
        job_status.already_up_to_date = true;
        job_status.bytes_done = job_package.info.image_length;
        unlock();
        op_event("SOTA;ALREADY_UP_TO_DATE;0");
        workflow_set(SENSOR_OTA_STEP_PREPARING_LINK, SENSOR_OTA_STEP_PASS);
        workflow_skip_transfer();
        workflow_set(SENSOR_OTA_STEP_VERIFYING, SENSOR_OTA_STEP_PASS);
        sensor_smp_client_end();
        return 0;
    }
    if (err != -EILSEQ && err != -EBADMSG) {
        upload_error_hint = err == -ENOENT ? SENSOR_OTA_ERROR_SMP_NOT_FOUND : SENSOR_OTA_ERROR_LINK_BUDGET;
        workflow_set(SENSOR_OTA_STEP_PREPARING_LINK, SENSOR_OTA_STEP_FAIL);
        sensor_smp_client_end();
        return err;
    }
    /* Scanning already stops for the whole connection (gatt_client); reported for parity. */
    workflow_set(SENSOR_OTA_STEP_PREPARING_LINK, SENSOR_OTA_STEP_PASS);
    op_event("SOTA;SCAN_PAUSED;1");
    report_link_value("LINK_MTU", et100_gatt_mtu());
    bool erase_first = !image_erased;
    if (erase_first) {
        upload_error_hint = SENSOR_OTA_ERROR_IMAGE_ERASE_FAILED;
        report_state(SENSOR_OTA_JOB_ERASING, 0);
        workflow_set(SENSOR_OTA_STEP_ERASING, SENSOR_OTA_STEP_ACTIVE);
    } else {
        upload_error_hint = SENSOR_OTA_ERROR_UPLOAD_TIMEOUT;
        workflow_set(SENSOR_OTA_STEP_ERASING, SENSOR_OTA_STEP_PASS);
        report_state(SENSOR_OTA_JOB_UPLOADING, job_status.bytes_done);
        workflow_set(SENSOR_OTA_STEP_UPLOADING, SENSOR_OTA_STEP_ACTIVE);
    }
    last_progress_ms = now_ms();
    err = sensor_smp_client_upload(job_package.info.image_length, job_package.info.upload_sha256,
                                   SENSOR_OTA_CHUNK_SIZE, true, erase_first, &bytes_done,
                                   progress_cb, erase_cb, NULL);
    if (err) {
        workflow_set(image_erased ? SENSOR_OTA_STEP_UPLOADING : SENSOR_OTA_STEP_ERASING, SENSOR_OTA_STEP_FAIL);
        sensor_smp_client_end();
        return err;
    }
    upload_completed = true;
    workflow_set(SENSOR_OTA_STEP_UPLOADING, SENSOR_OTA_STEP_PASS);
    report_state(SENSOR_OTA_JOB_TESTING_IMAGE, bytes_done);
    workflow_set(SENSOR_OTA_STEP_TESTING_IMAGE, SENSOR_OTA_STEP_ACTIVE);
    upload_error_hint = SENSOR_OTA_ERROR_IMAGE_TEST_FAILED;
    err = sensor_smp_client_image_state(images, 3, &count);
    if (err) report_stage_error("IMAGE_STATE", err);
    if (!err && !image_hash_present(images, count, job_package.info.mcuboot_image_hash, false)) {
        upload_error_hint = SENSOR_OTA_ERROR_HASH_MISMATCH;
        err = -EBADMSG;
        report_stage_error("IMAGE_HASH", err);
    }
    if (!err) {
        upload_error_hint = SENSOR_OTA_ERROR_IMAGE_TEST_FAILED;
        err = sensor_smp_client_image_test(job_package.info.mcuboot_image_hash);
        if (err) report_stage_error("IMAGE_TEST", err);
    }
    if (!err) {
        workflow_set(SENSOR_OTA_STEP_TESTING_IMAGE, SENSOR_OTA_STEP_PASS);
        report_state(SENSOR_OTA_JOB_RESETTING_TARGET, bytes_done);
        workflow_set(SENSOR_OTA_STEP_RESETTING_TARGET, SENSOR_OTA_STEP_ACTIVE);
        reset_sent = true;
        upload_error_hint = SENSOR_OTA_ERROR_RESET_FAILED;
        err = sensor_smp_client_os_reset();
        if (err) {
            report_stage_error("RESET", err);
            workflow_set(SENSOR_OTA_STEP_RESETTING_TARGET, SENSOR_OTA_STEP_FAIL);
        } else {
            workflow_set(SENSOR_OTA_STEP_RESETTING_TARGET, SENSOR_OTA_STEP_PASS);
        }
    } else {
        workflow_set(SENSOR_OTA_STEP_TESTING_IMAGE, SENSOR_OTA_STEP_FAIL);
    }
    sensor_smp_client_end();
    return err;
}

static int verify_run(void *ctx)
{
    enum sensor_ota_mismatch_field mismatch;
    (void)ctx;
    report_state(SENSOR_OTA_JOB_VERIFYING, job_status.bytes_done);
    workflow_set(SENSOR_OTA_STEP_VERIFYING, SENSOR_OTA_STEP_ACTIVE);
    int err = compare_target_metadata(&mismatch);
    if (err == -ENODATA) verify_error_hint = SENSOR_OTA_ERROR_METADATA_READ_FAILED;
    else if (err == -ENOTSUP) verify_error_hint = SENSOR_OTA_ERROR_CAPABILITY_MISMATCH;
    else if (err == -EXDEV || err == -EPROTONOSUPPORT) verify_error_hint = SENSOR_OTA_ERROR_TARGET_MISMATCH;
    else if (!err) {
        err = smp_open();
        if (!err) err = verify_target_firmware();
        sensor_smp_client_end();
        if (err == -ENOENT) verify_error_hint = SENSOR_OTA_ERROR_SMP_NOT_FOUND;
        else if (err == -EILSEQ) verify_error_hint = SENSOR_OTA_ERROR_VERSION_MISMATCH;
        else if (err == -EBADMSG) verify_error_hint = SENSOR_OTA_ERROR_HASH_MISMATCH;
        else if (err) verify_error_hint = SENSOR_OTA_ERROR_INTERNAL;
        else verify_error_hint = SENSOR_OTA_ERROR_NONE;
    }
    workflow_set(SENSOR_OTA_STEP_VERIFYING, err ? SENSOR_OTA_STEP_FAIL : SENSOR_OTA_STEP_PASS);
    return err;
}

/* ---- Job flow ---- */
static enum sensor_ota_job_error upload_error(int err)
{
    if (err == -ECANCELED) return SENSOR_OTA_ERROR_CANCELLED;
    if (err == -EACCES) return SENSOR_OTA_ERROR_AUTH_FAILED;
    if (err == -ENODATA) return SENSOR_OTA_ERROR_METADATA_READ_FAILED;
    if (err == -ENOTSUP) return SENSOR_OTA_ERROR_CAPABILITY_MISMATCH;
    if (err == -EXDEV || err == -EPROTONOSUPPORT) return SENSOR_OTA_ERROR_TARGET_MISMATCH;
    if (err == -ETIMEDOUT)
        return upload_error_hint != SENSOR_OTA_ERROR_NONE ? upload_error_hint : SENSOR_OTA_ERROR_UPLOAD_TIMEOUT;
    if (err == -EOVERFLOW || err == -EALREADY) return SENSOR_OTA_ERROR_OFFSET_INVALID;
    if (err == -ESTALE) return SENSOR_OTA_ERROR_PACKAGE_INVALID;
    if (upload_error_hint != SENSOR_OTA_ERROR_NONE) return upload_error_hint;
    return SENSOR_OTA_ERROR_CONNECT_FAILED;
}

static bool upload_result_is_final(int err)
{
    enum sensor_ota_job_error hint = upload_error_hint;
    return err == 0 || upload_completed || reset_sent || err == -ECANCELED || err == -ESTALE ||
           err == -EXDEV || err == -EPROTONOSUPPORT || err == -ENOTSUP ||
           hint == SENSOR_OTA_ERROR_PACKAGE_INVALID || hint == SENSOR_OTA_ERROR_AUTH_FAILED ||
           hint == SENSOR_OTA_ERROR_METADATA_READ_FAILED || hint == SENSOR_OTA_ERROR_SMP_NOT_FOUND;
}

static bool verify_result_is_final(int err)
{
    return err == 0 || err == -ECANCELED || err == -EACCES || err == -EILSEQ || err == -EBADMSG ||
           err == -ENODATA || err == -ENOTSUP || err == -EXDEV || err == -EPROTONOSUPPORT ||
           verify_error_hint == SENSOR_OTA_ERROR_SMP_NOT_FOUND;
}

static void delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static int verify_after_reboot(void)
{
    uint32_t start = now_ms();
    int err = -ETIMEDOUT;
    while (!cancel_requested && (uint32_t)(now_ms() - start) < SENSOR_OTA_REBOOT_ADV_MS) {
        verify_error_hint = SENSOR_OTA_ERROR_WAIT_ADV_TIMEOUT;
        verify_ops.wait_ms = SENSOR_OTA_REBOOT_ADV_MS - (now_ms() - start);
        err = et100_gatt_exclusive(job_status.target_mac, &verify_ops);
        if (verify_result_is_final(err)) return err;
        delay_ms(500);   /* A rebooting Sensor may advertise before its connect path is ready. */
    }
    return cancel_requested ? -ECANCELED : -ETIMEDOUT;
}

static void finish_job(enum sensor_ota_job_state state, enum sensor_ota_job_error error)
{
    char data[56];
    enum sensor_ota_step_result result = state == SENSOR_OTA_JOB_SUCCESS ? SENSOR_OTA_STEP_PASS
                                                                         : SENSOR_OTA_STEP_FAIL;
    lock();
    enum sensor_ota_workflow_step step = job_status.workflow_step;
    bool step_failed = job_status.failed_step != SENSOR_OTA_STEP_NONE;
    unlock();
    if (state != SENSOR_OTA_JOB_SUCCESS && !step_failed && step != SENSOR_OTA_STEP_NONE)
        workflow_set(step, SENSOR_OTA_STEP_FAIL);
    lock();
    finalizing = 1;
    job_status.elapsed_ms = now_ms() - job_started_ms;
    job_status.workflow_step = SENSOR_OTA_STEP_RESULT;
    job_status.step_result = result;
    sensor_ota_workflow_record(&job_status.step_results, &job_status.failed_step,
                               SENSOR_OTA_STEP_RESULT, result);
    job_status.state = state;
    job_status.error = error;
    unlock();
    snprintf(data, sizeof data, "SOTA;%s;%u", sensor_ota_job_state_name(state), (unsigned)error);
    op_event(data);
    snprintf(data, sizeof data, "SOTA;STEP;RESULT;%s", sensor_ota_step_result_name(result));
    op_event(data);
    op_event("SOTA;SCAN_PAUSED;0");
    ota_worker_relay_end();
    et100_gatt_unreserve();
    finalizing = 0;
}

static void run_job(void)
{
    int err = -EIO;
    uint32_t resume_start = 0;
    bool resume_started = false;
    workflow_set(SENSOR_OTA_STEP_PACKAGE_CHECK, SENSOR_OTA_STEP_PASS);
    for (unsigned attempt = 0; attempt < SENSOR_OTA_MAX_SESSIONS; attempt++) {
        if (cancel_requested) { err = -ECANCELED; break; }
        upload_ops.wait_ms = attempt == 0 ? SENSOR_OTA_FIRST_ADV_MS : SENSOR_OTA_RESUME_ADV_MS;
        upload_error_hint = SENSOR_OTA_ERROR_WAIT_ADV_TIMEOUT;
        workflow_set(SENSOR_OTA_STEP_WAIT_ADV, SENSOR_OTA_STEP_ACTIVE);
        report_state(SENSOR_OTA_JOB_WAIT_ADV, attempt);
        err = et100_gatt_exclusive(job_status.target_mac, &upload_ops);
        if (upload_result_is_final(err)) break;
        /* Do not reuse another packet from the same short OTA advertising burst. */
        delay_ms(SENSOR_OTA_RETRY_BACKOFF_MS);
        if (!resume_started) { resume_start = now_ms(); resume_started = true; }
        if ((uint32_t)(now_ms() - resume_start) >= SENSOR_OTA_RESUME_ADV_MS) { err = -ETIMEDOUT; break; }
    }
    if (cancel_requested || err == -ECANCELED) {
        finish_job(SENSOR_OTA_JOB_CANCELLED, SENSOR_OTA_ERROR_CANCELLED);
        return;
    }
    if (err) {
        finish_job(err == -ETIMEDOUT ? SENSOR_OTA_JOB_TIMEOUT : SENSOR_OTA_JOB_FAILED, upload_error(err));
        return;
    }
    if (job_status.already_up_to_date) {
        finish_job(SENSOR_OTA_JOB_SUCCESS, SENSOR_OTA_ERROR_NONE);
        return;
    }
    report_state(SENSOR_OTA_JOB_WAIT_REBOOT, job_status.bytes_done);
    workflow_set(SENSOR_OTA_STEP_WAIT_REBOOT, SENSOR_OTA_STEP_ACTIVE);
    err = verify_after_reboot();
    if (cancel_requested || err == -ECANCELED) finish_job(SENSOR_OTA_JOB_CANCELLED, SENSOR_OTA_ERROR_CANCELLED);
    else if (err == -ETIMEDOUT) finish_job(SENSOR_OTA_JOB_TIMEOUT, SENSOR_OTA_ERROR_REBOOT_TIMEOUT);
    else if (err == -EACCES) finish_job(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_AUTH_FAILED);
    else if (err == -ENODATA) finish_job(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_METADATA_READ_FAILED);
    else if (err == -ENOTSUP) finish_job(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_CAPABILITY_MISMATCH);
    else if (err == -EXDEV || err == -EPROTONOSUPPORT) finish_job(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_TARGET_MISMATCH);
    else if (verify_error_hint == SENSOR_OTA_ERROR_SMP_NOT_FOUND) finish_job(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_SMP_NOT_FOUND);
    else if (err == -EILSEQ) finish_job(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_VERSION_MISMATCH);
    else if (err == -EBADMSG) finish_job(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_HASH_MISMATCH);
    else if (err) finish_job(SENSOR_OTA_JOB_FAILED, SENSOR_OTA_ERROR_INTERNAL);
    else finish_job(SENSOR_OTA_JOB_SUCCESS, SENSOR_OTA_ERROR_NONE);
}

int sensor_ota_job_poll(void)
{
    if (!start_pending) return 0;
    start_pending = 0;
    run_job();
    return 1;
}

void sensor_ota_job_init(void)
{
    et100_gatt_set_idle_hook(sensor_ota_job_poll);
}

static bool target_mac_valid(const uint8_t mac[6])
{
    bool any_nonzero = false, any_not_ff = false;
    for (size_t i = 0; i < 6; i++) { any_nonzero |= mac[i] != 0; any_not_ff |= mac[i] != 0xff; }
    return any_nonzero && any_not_ff;
}

int sensor_ota_job_start(const uint8_t mac[6])
{
    struct sensor_store_status package;
    if (!mac || !target_mac_valid(mac)) return -EINVAL;
    if (sensor_ota_job_busy() || start_pending || et100_gatt_state() != 0) return -EBUSY;
    if (!ota_worker_sensor_status(&package)) return -ENOTSUP;
    if (package.state == SENSOR_STORE_EMPTY) return -ENODATA;
    if (package.state != SENSOR_STORE_VALID) return -EBADMSG;
    if (!sensor_ota_compat_package_supported(&package.info)) return -EXDEV;
    if (et100_gatt_reserve()) return -EBUSY;
    if (!ota_worker_relay_begin()) { et100_gatt_unreserve(); return -EBUSY; }
    lock();
    memset(&job_status, 0, sizeof job_status);
    job_status.state = SENSOR_OTA_JOB_WAIT_ADV;
    memcpy(job_status.target_mac, mac, 6);
    job_status.generation = package.generation;
    job_status.image_length = package.info.image_length;
    job_package = package;
    job_started_ms = now_ms();
    unlock();
    reset_sent = upload_completed = image_erased = false;
    upload_error_hint = verify_error_hint = SENSOR_OTA_ERROR_NONE;
    cache_len = 0;
    cancel_requested = 0;
    upload_ops = (struct et100_gatt_exclusive_ops){ SENSOR_OTA_FIRST_ADV_MS, upload_matched, stage_cb,
                                                    upload_run, &upload_ops };
    verify_ops = (struct et100_gatt_exclusive_ops){ SENSOR_OTA_REBOOT_ADV_MS, verify_matched, stage_cb,
                                                    verify_run, &verify_ops };
    start_pending = 1;
    return 0;
}

int sensor_ota_job_cancel(void)
{
    if (!sensor_ota_job_busy()) return -EALREADY;
    cancel_requested = 1;
    et100_gatt_cancel();
    return 0;
}

bool sensor_ota_job_busy(void)
{
    lock();
    bool busy = finalizing || state_active(job_status.state);
    unlock();
    return busy;
}

void sensor_ota_job_get_status(struct sensor_ota_job_status *status)
{
    if (!status) return;
    lock();
    *status = job_status;
    if (state_active(status->state)) status->elapsed_ms = now_ms() - job_started_ms;
    unlock();
}

/* ---- RS232 (gateway rs232_cmd_handler.c formats) ---- */
static int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int parse_mac(const char *text, uint8_t mac[6])
{
    if (strlen(text) != 12) return -1;
    for (unsigned i = 0; i < 6; i++) {
        int hi = nibble(text[2 * i]), lo = nibble(text[2 * i + 1]);
        if (hi < 0 || lo < 0) return -1;
        mac[i] = (uint8_t)(hi << 4 | lo);
    }
    return 0;
}

static enum sensor_ota_job_error start_error(int ret)
{
    if (ret == -ENOTSUP) return SENSOR_OTA_ERROR_UNSUPPORTED;
    if (ret == -ENODATA) return SENSOR_OTA_ERROR_NO_PACKAGE;
    if (ret == -EBADMSG) return SENSOR_OTA_ERROR_PACKAGE_INVALID;
    if (ret == -EBUSY) return SENSOR_OTA_ERROR_BUSY;
    if (ret == -EINVAL) return SENSOR_OTA_ERROR_BAD_MAC;
    if (ret == -EXDEV) return SENSOR_OTA_ERROR_TARGET_MISMATCH;
    return SENSOR_OTA_ERROR_INTERNAL;
}

static void mac_hex(const uint8_t mac[6], char out[13])
{
    snprintf(out, 13, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int sensor_ota_job_uart(const char *cmd, const char *params, const char *token, char *out, size_t capacity)
{
    struct sensor_ota_job_status st;
    char mac[13];
    int n;
    if (!strcmp(cmd, "SENSOROTA")) {
        uint8_t target[6];
        int ret = strchr(params, ';') || parse_mac(params, target) ? -EINVAL : sensor_ota_job_start(target);
        if (!ret) {
            sensor_ota_job_get_status(&st);
            n = snprintf(out, capacity, "AT$PD=BLEGW$SENSOROTA;1;%s;%u;%u;%s\r\n", params,
                         (unsigned)st.generation, (unsigned)st.image_length, token);
        } else {
            enum sensor_ota_job_error e = start_error(ret);
            n = snprintf(out, capacity, "AT$PD=BLEGW$SENSOROTA;0;%u;%s;%s\r\n", (unsigned)e,
                         sensor_ota_job_error_name(e), token);
        }
    } else if (*params) {
        return 0;   /* the query/cancel commands take no parameters */
    } else if (!strcmp(cmd, "GSENSOROTA")) {
        sensor_ota_job_get_status(&st);
        mac_hex(st.target_mac, mac);
        const char *detail = st.state == SENSOR_OTA_JOB_SUCCESS && st.already_up_to_date
                             ? "ALREADY_UP_TO_DATE" : sensor_ota_job_error_name(st.error);
        n = snprintf(out, capacity, "AT$PD=BLEGW$GSENSOROTA;1;%s;%s;%u;%u;%u;%u;%u;%s;%s\r\n",
                     sensor_ota_job_state_name(st.state), mac, (unsigned)st.generation,
                     (unsigned)st.bytes_done, (unsigned)st.image_length, (unsigned)st.elapsed_ms,
                     (unsigned)st.error, detail, token);
    } else if (!strcmp(cmd, "GSENSOROTAFLOW")) {
        sensor_ota_job_get_status(&st);
        /* newlib-nano does not consume ll conversions; keep the 48-bit wire field. */
        n = snprintf(out, capacity, "AT$PD=BLEGW$GSENSOROTAFLOW;1;%s;%s;%s;%s;%08lX;%04lX%08lX;%s\r\n",
                     sensor_ota_job_state_name(st.state), sensor_ota_workflow_step_name(st.workflow_step),
                     sensor_ota_step_result_name(st.step_result), sensor_ota_workflow_step_name(st.failed_step),
                     (unsigned long)st.target.match_mask,
                     (unsigned long)((st.step_results >> 32) & 0xffffu),
                     (unsigned long)(st.step_results & 0xffffffffu), token);
    } else if (!strcmp(cmd, "GSENSOROTAMETA")) {
        sensor_ota_job_get_status(&st);
        if (!st.target.valid) {
            n = snprintf(out, capacity, "AT$PD=BLEGW$GSENSOROTAMETA;0;NOT_READY;%s\r\n", token);
        } else {
            const struct eup_gatt_metadata *m = &st.target.sensor;
            mac_hex(st.target_mac, mac);
            n = snprintf(out, capacity,
                         "AT$PD=BLEGW$GSENSOROTAMETA;1;%s;%s;%s;%s;%s;%s;%s;%s;%08lX;%02X;%s;%s\r\n",
                         mac, m->project_name, m->hardware_version, m->fw_version, m->build_id, m->soc,
                         st.target.board, m->device_id, (unsigned long)m->capabilities, m->aiot_role,
                         sensor_ota_mismatch_field_name(st.target.mismatch_field), token);
        }
    } else if (!strcmp(cmd, "CSENSOROTA")) {
        int ret = sensor_ota_job_cancel();
        n = snprintf(out, capacity, "AT$PD=BLEGW$CSENSOROTA;%u;%s;%s\r\n", ret == 0, ret == 0 ? "OK" : "NOT_ACTIVE",
                     token);
    } else {
        return 0;
    }
    return n > 0 && (size_t)n < capacity ? n : 0;
}
