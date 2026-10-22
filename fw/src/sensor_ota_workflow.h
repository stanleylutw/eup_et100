#ifndef ET100_SENSOR_OTA_WORKFLOW_H
#define ET100_SENSOR_OTA_WORKFLOW_H
/* v0.8.9: 15-step Sensor OTA workflow, identical to eup_gateway
 * src/ota/sensor_ota_workflow.h (GSENSOROTAFLOW packs 3 bits per step). */
#include <stdint.h>

enum sensor_ota_workflow_step {
    SENSOR_OTA_STEP_NONE = 0,
    SENSOR_OTA_STEP_PACKAGE_CHECK,
    SENSOR_OTA_STEP_WAIT_ADV,
    SENSOR_OTA_STEP_CONNECTING,
    SENSOR_OTA_STEP_CONNECTED,
    SENSOR_OTA_STEP_AUTHENTICATING,
    SENSOR_OTA_STEP_CHECKING_TARGET,
    SENSOR_OTA_STEP_PREPARING_LINK,
    SENSOR_OTA_STEP_ERASING,
    SENSOR_OTA_STEP_UPLOADING,
    SENSOR_OTA_STEP_TESTING_IMAGE,
    SENSOR_OTA_STEP_RESETTING_TARGET,
    SENSOR_OTA_STEP_WAIT_REBOOT,
    SENSOR_OTA_STEP_RECONNECTING,
    SENSOR_OTA_STEP_VERIFYING,
    SENSOR_OTA_STEP_RESULT,
};

enum sensor_ota_step_result {
    SENSOR_OTA_STEP_PENDING = 0,
    SENSOR_OTA_STEP_ACTIVE,
    SENSOR_OTA_STEP_PASS,
    SENSOR_OTA_STEP_FAIL,
    SENSOR_OTA_STEP_SKIPPED,
};

/* Three bits per step, starting with PACKAGE_CHECK at bits 0..2. */
static inline enum sensor_ota_step_result sensor_ota_workflow_result(
    uint64_t results, enum sensor_ota_workflow_step step)
{
    if (step < SENSOR_OTA_STEP_PACKAGE_CHECK || step > SENSOR_OTA_STEP_RESULT)
        return SENSOR_OTA_STEP_PENDING;
    return (enum sensor_ota_step_result)((results >> (3u * (step - 1u))) & 7u);
}

static inline void sensor_ota_workflow_record(uint64_t *results,
    enum sensor_ota_workflow_step *failed_step, enum sensor_ota_workflow_step step,
    enum sensor_ota_step_result result)
{
    if (step < SENSOR_OTA_STEP_PACKAGE_CHECK || step > SENSOR_OTA_STEP_RESULT) return;
    unsigned shift = 3u * (step - 1u);
    if (result == SENSOR_OTA_STEP_ACTIVE &&
        (step == SENSOR_OTA_STEP_WAIT_ADV || step == SENSOR_OTA_STEP_WAIT_REBOOT)) {
        /* A new session replaces failed/future evidence while retaining prior work. */
        *results &= ((uint64_t)1 << shift) - 1u;
        *failed_step = SENSOR_OTA_STEP_NONE;
    }
    *results = (*results & ~((uint64_t)7 << shift)) | ((uint64_t)result << shift);
    if (result == SENSOR_OTA_STEP_FAIL && *failed_step == SENSOR_OTA_STEP_NONE)
        *failed_step = step;
    else if (result == SENSOR_OTA_STEP_PASS && *failed_step == step)
        *failed_step = SENSOR_OTA_STEP_NONE;
}
#endif
