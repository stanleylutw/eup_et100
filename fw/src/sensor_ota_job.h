#ifndef ET100_SENSOR_OTA_JOB_H
#define ET100_SENSOR_OTA_JOB_H
/* v0.8.9 production Sensor OTA relay: NOR package -> GATT SMP -> Sensor.
 * Ported from eup_gateway src/ota/sensor_ota_job.c; states, errors, the
 * 15-step workflow, timeouts and the RS232 wire format (gateway protocol
 * §9.9: SENSOROTA / GSENSOROTA / CSENSOROTA / GSENSOROTAFLOW /
 * GSENSOROTAMETA) are identical. The job runs inside the GATT worker (idle
 * hook) and holds the single GATT owner and the OTA manager for its lifetime. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "eup_aiot_metadata.h"
#include "sensor_ota_compat.h"
#include "sensor_ota_workflow.h"

enum sensor_ota_job_state {
    SENSOR_OTA_JOB_IDLE = 0,
    SENSOR_OTA_JOB_WAIT_ADV,
    SENSOR_OTA_JOB_CONNECTING,
    SENSOR_OTA_JOB_ERASING,
    SENSOR_OTA_JOB_UPLOADING,
    SENSOR_OTA_JOB_TESTING_IMAGE,
    SENSOR_OTA_JOB_RESETTING_TARGET,
    SENSOR_OTA_JOB_WAIT_REBOOT,
    SENSOR_OTA_JOB_VERIFYING,
    SENSOR_OTA_JOB_SUCCESS,
    SENSOR_OTA_JOB_FAILED,
    SENSOR_OTA_JOB_CANCELLED,
    SENSOR_OTA_JOB_TIMEOUT,
};

enum sensor_ota_job_error {
    SENSOR_OTA_ERROR_NONE = 0,
    SENSOR_OTA_ERROR_UNSUPPORTED = 1,
    SENSOR_OTA_ERROR_NO_PACKAGE = 2,
    SENSOR_OTA_ERROR_PACKAGE_INVALID = 3,
    SENSOR_OTA_ERROR_BUSY = 4,
    SENSOR_OTA_ERROR_BAD_MAC = 5,
    SENSOR_OTA_ERROR_WAIT_ADV_TIMEOUT = 6,
    SENSOR_OTA_ERROR_CONNECT_FAILED = 7,
    SENSOR_OTA_ERROR_AUTH_FAILED = 8,
    SENSOR_OTA_ERROR_TARGET_MISMATCH = 9,
    SENSOR_OTA_ERROR_SMP_NOT_FOUND = 10,
    SENSOR_OTA_ERROR_LINK_BUDGET = 11,
    SENSOR_OTA_ERROR_UPLOAD_TIMEOUT = 12,
    SENSOR_OTA_ERROR_OFFSET_INVALID = 13,
    SENSOR_OTA_ERROR_IMAGE_TEST_FAILED = 14,
    SENSOR_OTA_ERROR_RESET_FAILED = 15,
    SENSOR_OTA_ERROR_REBOOT_TIMEOUT = 16,
    SENSOR_OTA_ERROR_VERSION_MISMATCH = 17,
    SENSOR_OTA_ERROR_HASH_MISMATCH = 18,
    SENSOR_OTA_ERROR_CANCELLED = 19,
    SENSOR_OTA_ERROR_INTERNAL = 20,
    SENSOR_OTA_ERROR_IMAGE_ERASE_FAILED = 21,
    SENSOR_OTA_ERROR_METADATA_READ_FAILED = 22,
    SENSOR_OTA_ERROR_CAPABILITY_MISMATCH = 23,
};

/* Timing and transfer parameters: gateway values (D5), revisit after M-R1. */
#define SENSOR_OTA_CHUNK_SIZE 192u
#define SENSOR_OTA_FIRST_ADV_MS 180000u
#define SENSOR_OTA_RESUME_ADV_MS 60000u
#define SENSOR_OTA_REBOOT_ADV_MS 90000u
#define SENSOR_OTA_MAX_SESSIONS 3u
#define SENSOR_OTA_RETRY_BACKOFF_MS 1200u

struct sensor_ota_target_metadata {
    struct eup_gatt_metadata sensor;
    char board[24];
    uint32_t match_mask;
    enum sensor_ota_mismatch_field mismatch_field;
    bool valid;
};

struct sensor_ota_job_status {
    enum sensor_ota_job_state state;
    enum sensor_ota_job_error error;
    enum sensor_ota_workflow_step workflow_step;
    enum sensor_ota_step_result step_result;
    enum sensor_ota_workflow_step failed_step;
    uint64_t step_results;
    bool already_up_to_date;
    uint8_t target_mac[6];
    uint32_t generation, bytes_done, image_length, elapsed_ms;
    struct sensor_ota_target_metadata target;
};

/* Register the GATT idle hook (after et100_gatt_init and ota_worker_start). */
void sensor_ota_job_init(void);
/* 0, -EINVAL bad MAC, -EBUSY, -ENOTSUP, -ENODATA no package, -EBADMSG invalid, -EXDEV board. */
int sensor_ota_job_start(const uint8_t mac[6]);
int sensor_ota_job_cancel(void);
bool sensor_ota_job_busy(void);
void sensor_ota_job_get_status(struct sensor_ota_job_status *status);
/* GATT idle hook: runs a started job to completion; nonzero if it ran one. */
int sensor_ota_job_poll(void);
/* RS232: one of the five commands -> response line length (with CRLF); 0 = not ours. */
int sensor_ota_job_uart(const char *cmd, const char *params, const char *token, char *out, size_t capacity);

const char *sensor_ota_job_state_name(enum sensor_ota_job_state state);
const char *sensor_ota_job_error_name(enum sensor_ota_job_error error);
const char *sensor_ota_workflow_step_name(enum sensor_ota_workflow_step step);
const char *sensor_ota_step_result_name(enum sensor_ota_step_result result);
#endif
