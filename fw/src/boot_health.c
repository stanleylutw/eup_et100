#include "boot_health.h"
#include "boot_state.h"
#include "mcuboot_image.h"
#include "mbedtls/sha256.h"
#include "fr30xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fdb_app.h"
#include "et100_fdb_keys.h"
#include "ota_worker.h"
#include "eup_fw_info.h"
#include <stdio.h>
#include <string.h>

static volatile unsigned advertising_started;
static TickType_t started,last_feed,last_poll;
static bool initialized,trial,storage_ok;
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
static bool confirmed;
#endif
static uint8_t own_hash[32];
static TickType_t trial_started,timeout_started;
static bool trial_boot,timed_out,timeout_queued,own_valid,boot_ready;
static struct boot_record boot;
static char pending_line[INST_LINE_MAX+1];
static bool report_storage_ready,pending_mark,delivered_mark;
static uint8_t pending_hash[32],delivered_hash[32];
static void event(const char *name,const char *fields)
{
    pending_mark=false;
    if (!inst_line(pending_line,sizeof pending_line,'A',name,boot.kind ? boot.hash : NULL,fields)) pending_line[0]=0;
}
static bool result_reported(const uint8_t hash[32])
{
    uint8_t stored[32];
    return report_storage_ready && flashdb_get_length(ET100_FDB_INST_REPORTED_KEY)==sizeof stored &&
        flashdb_get(ET100_FDB_INST_REPORTED_KEY,stored,sizeof stored)==sizeof stored &&
        !memcmp(stored,hash,sizeof stored);
}
static void mark_after_send(const uint8_t hash[32])
{
    if (pending_line[0]) { memcpy(pending_hash,hash,32);pending_mark=true; }
}
size_t et100_boot_line(char *out,size_t capacity)
{
    size_t n=strlen(pending_line);
    if (!n || n>=capacity) return 0;
    memcpy(out,pending_line,n+1);pending_line[0]=0;
    delivered_mark=pending_mark;
    if (pending_mark) memcpy(delivered_hash,pending_hash,32);
    pending_mark=false;return n;
}
void et100_boot_line_sent(void)
{
    /* Called only after serial_send returns, never on enqueue or in a callback.
     * Keep the delivered transaction even if worker refreshes boot meanwhile. */
    if (!delivered_mark) return;
    delivered_mark=false;
    if (report_storage_ready && !result_reported(delivered_hash))
        (void)flashdb_set(ET100_FDB_INST_REPORTED_KEY,delivered_hash,sizeof delivered_hash);
}
int et100_boot_status(struct inst_snapshot *out)
{
    struct boot_record current;
    memset(out,0,sizeof *out);
    taskENTER_CRITICAL();
    current=boot;out->state=current.kind;out->ready=boot_ready && own_valid;
    memcpy(out->running_hash,own_hash,32);
    taskEXIT_CRITICAL();
    out->has_record=ota_worker_inst_record(&out->record);
    if (out->has_record && memcmp(out->record.hash,current.hash,32)) out->has_record=false;
    /* NOR init failure cannot persist an ABORT record to that same NOR. Boot
     * state still proves the terminal outcome; do not invent its missing cause. */
    unsigned terminal=current.last_update==UPDATE_ABORTED ? IR_ABORTED : IR_ROLLED_BACK;
    if (current.kind==BOOT_CONFIRMED && current.last_update!=UPDATE_OK &&
        (!out->has_record || out->record.result!=terminal)) {
        memset(&out->record,0,sizeof out->record);memcpy(out->record.hash,current.hash,32);
        out->record.state=current.kind;out->record.result=terminal;out->record.code=IC_RESULT_UNAVAILABLE;
        out->has_record=true;
    }
    return out->ready;
}
static bool save_result(unsigned result,unsigned code)
{
    struct inst_record record={0};
    (void)ota_worker_inst_record(&record);
    memcpy(record.hash,boot.hash,32);record.state=boot.kind;
    record.result=result;record.code=code;record.stage=IS_BOOT;record.attempts=0;
    return ota_worker_inst_save(&record);
}
static int read_flash(uint32_t off,void *data,size_t n)
{
    if (off>0x200000 || n>0x200000-off) return -1;
    memcpy(data,(const void *)(0x08000000u+off),n);return 0;
}
static bool trial_expired(void)
{
    return timed_out || (trial_boot && boot.kind==BOOT_TRIAL &&
        (TickType_t)(xTaskGetTickCount()-trial_started)>=pdMS_TO_TICKS(120000));
}
static int erase_state(uint32_t off,size_t n)
{
    /* All App boot-state transactions hold the same critical section. Reject
     * late SMP confirm too, including the grace period before timeout reset. */
    if (trial_expired()) return -1;
    if (off<BOOT_STATE_BASE || off>=0x200000 || n>0x200000-off || (off&4095) || (n&4095)) return -1;
    flash_erase(QSPI0,off,n);
    const volatile uint8_t *p=(const volatile uint8_t *)(0x08000000u+off);
    for (size_t i=0;i<n;i++) if (p[i]!=0xff) return -1;
    return 0;
}
static int program_state(uint32_t off,const void *data,size_t n)
{
    if (trial_expired()) return -1;
    if (off<BOOT_STATE_BASE || off>=0x200000 || n>0x200000-off || n>256) return -1;
    uint8_t copy[256];memcpy(copy,data,n);flash_write(QSPI0,off,n,copy);
    return memcmp((const void *)(0x08000000u+off),data,n) ? -1 : 0;
}
static const struct internal_port port={read_flash,erase_state,program_state};
const struct internal_port *et100_boot_port(void) { return &port; }
void et100_boot_refresh(void)
{
    struct boot_record current;bool valid=boot_state_read(&port,&current)==0;
    taskENTER_CRITICAL();boot_ready=valid;boot=current;
    if (valid && own_valid && current.kind==BOOT_CONFIRMED && !memcmp(current.confirmed_hash,own_hash,32)) {
        trial_boot=false;
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
        confirmed=true;
#endif
    }
    taskEXIT_CRITICAL();
}
void et100_boot_adv_started(void) { advertising_started=1; }
void et100_boot_poll(int storage_ready)
{
    report_storage_ready=storage_ready!=0;
    TickType_t now=xTaskGetTickCount();
    if (initialized && (TickType_t)(now-last_poll)>pdMS_TO_TICKS(2000)) started=now;
    last_poll=now;
    if (!initialized) {
        initialized=true;started=trial_started=now;last_feed=now;
        struct boot_record state;struct image_info info;
        boot_ready=boot_state_read(&port,&state)==0;
        boot=state;trial_boot=boot_ready && state.kind==BOOT_TRIAL;
        own_valid=mcuboot_image_verify((const uint8_t *)ET100_APP_SLOT,ET100_APP_SLOT_SIZE,&info)==0 &&
            mbedtls_sha256_ret((const uint8_t *)ET100_APP_SLOT,info.size,own_hash,0)==0;
        if (trial_boot && own_valid && info.size==state.length && !memcmp(own_hash,state.hash,32)) {
            trial=true;
            if (storage_ready) {
                uint32_t marker=state.sequence,verify=0;
                storage_ok=flashdb_set(ET100_FDB_BOOT_HEALTH_KEY,(uint8_t *)&marker,4)==FDB_NO_ERR &&
                    flashdb_get(ET100_FDB_BOOT_HEALTH_KEY,(uint8_t *)&verify,4)==4 && verify==marker;
            }
        }
        bool terminal=boot_ready && state.kind==BOOT_CONFIRMED &&
            (state.last_update==UPDATE_FAILED || state.last_update==UPDATE_ABORTED);
        if (trial_boot || (terminal && !result_reported(state.hash))) {
            struct inst_snapshot status;(void)et100_boot_status(&status);
            char fields[72];snprintf(fields,sizeof fields,"%s;%s;%s",inst_state_name(state.kind),
                EUP_FIRMWARE_VERSION,status.has_record ? inst_result_name(status.record.result) : "-");
            event("APP",fields);
            if (terminal) mark_after_send(state.hash);
        }
    }
    if (trial_boot && !timed_out && (TickType_t)(now-trial_started)>=pdMS_TO_TICKS(120000)) {
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
        if (!confirmed)
#endif
        {
            timed_out=true;timeout_started=now;
            const char *reason=!trial ? "WRITE" : !storage_ready || !storage_ok ? "STORAGE" : !advertising_started ? "BLE" :
                (TickType_t)(now-started)<pdMS_TO_TICKS(30000) ? "LOOP" : "WRITE";
            char fields[48];snprintf(fields,sizeof fields,"%lu;%s",(unsigned long)((now-trial_started)*portTICK_PERIOD_MS/1000),reason);
            event("TRIAL_TIMEOUT",fields);
        }
    }
    if (timed_out) {
        if (!timeout_queued) timeout_queued=save_result(IR_TIMEOUT,IC_TRIAL_TIMEOUT);
        /* Leave at most one second for worker persistence and serial event draining. */
        if ((timeout_queued && !ota_worker_inst_pending() && !pending_line[0]) ||
            (TickType_t)(now-timeout_started)>=pdMS_TO_TICKS(1000)) NVIC_SystemReset();
        return; /* No more WDT refresh while waiting for reset. */
    }
#if defined(ET100_TEST_HANG) && ET100_TEST_HANG
    if (trial && (TickType_t)(now-started)>=pdMS_TO_TICKS(5000)) {
        __disable_irq();for (;;) __NOP(); /* Deliberate test: no other task can feed. */
    }
#endif
    if ((TickType_t)(now-last_feed)>=pdMS_TO_TICKS(1000)) {
        if (ool_read(PMU_REG_WDT_CTRL)&PMU_WDT_EN_BIT) iwdt_Refresh();
        last_feed=now;
    }
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
    if (trial && !confirmed && storage_ready && storage_ok && advertising_started &&
        (TickType_t)(now-started)>=pdMS_TO_TICKS(30000)) {
        /* Serialize both-bank transaction against scheduling and flash users. */
        taskENTER_CRITICAL();confirmed=boot_state_confirm(&port,own_hash)==0;taskEXIT_CRITICAL();
        if (confirmed) {
            taskENTER_CRITICAL();boot.kind=BOOT_CONFIRMED;taskEXIT_CRITICAL();
            event("CONFIRMED","");
            mark_after_send(own_hash);
            (void)save_result(IR_CONFIRMED,IC_OK);
        }
    }
#endif
}
