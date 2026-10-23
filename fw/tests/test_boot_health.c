/* TRIAL boot storage probe must not touch other FlashEnv records (auto list). */
#include <assert.h>
#include <stdio.h>
#include "../src/boot_health.c"

static TickType_t ticks;
static unsigned confirms;
static bool fail_confirm;
static unsigned resets, feeds;
static unsigned read_kind=BOOT_TRIAL;
static unsigned read_update=UPDATE_OK;
static uint8_t read_hash[32];
static bool fail_report_write;
static struct inst_record cached;
bool ota_worker_inst_record(struct inst_record *out) { *out=cached;return cached.result!=IR_NONE; }
bool ota_worker_inst_save(const struct inst_record *r) { cached=*r;return true; }
bool ota_worker_inst_pending(void) { return false; }
void NVIC_SystemReset(void) { resets++; }
static struct { uint32_t key; uint8_t data[128]; size_t size; } db[8];
static unsigned db_count;

TickType_t xTaskGetTickCount(void) { return ticks; }
void flash_erase(void *q,uint32_t o,uint32_t n) { (void)q;(void)o;(void)n;assert(0); }
void flash_write(void *q,uint32_t o,uint32_t n,uint8_t *d) { (void)q;(void)o;(void)n;(void)d;assert(0); }
uint8_t ool_read(uint8_t reg) { (void)reg;return PMU_WDT_EN_BIT; }
void iwdt_Refresh(void) { feeds++; }
int boot_state_read(const struct internal_port *p,struct boot_record *r)
{ (void)p;memset(r,0,sizeof *r);r->sequence=0x5a5a0007u;r->kind=read_kind;r->length=1024;
  r->last_update=read_update;memcpy(r->hash,read_hash,32);return 0; }
int boot_state_confirm(const struct internal_port *p,const uint8_t hash[32])
{ (void)p;(void)hash;confirms++;return fail_confirm ? -1 : 0; }
int mcuboot_image_verify(const uint8_t *image,size_t available,struct image_info *info)
{ (void)image;(void)available;memset(info,0,sizeof *info);info->size=1024;return 0; }
int mbedtls_sha256_ret(const unsigned char *in,size_t n,unsigned char out[32],int is224)
{ (void)in;(void)n;(void)is224;memset(out,0,32);return 0; }

static int slot(uint32_t key)
{ for (unsigned i=0;i<db_count;i++) if (db[i].key==key) return (int)i; return -1; }
size_t flashdb_get_length(uint32_t key) { int i=slot(key);return i<0 ? 0 : db[i].size; }
size_t flashdb_get(uint32_t key,uint8_t *data,uint32_t length)
{ int i=slot(key);if(i<0||length<db[i].size)return 0;memcpy(data,db[i].data,db[i].size);return db[i].size; }
int flashdb_set(uint32_t key,uint8_t *data,uint32_t length)
{
    if (key==ET100_FDB_INST_REPORTED_KEY && fail_report_write) return -1;
    int i=slot(key);if(i<0){assert(db_count<8);i=(int)db_count++;db[i].key=key;}
    assert(length<=sizeof db[i].data);memcpy(db[i].data,data,length);db[i].size=length;return FDB_NO_ERR;
}

static void reboot_health(unsigned kind,unsigned update)
{
    initialized=trial=trial_boot=timed_out=timeout_queued=storage_ok=own_valid=boot_ready=false;
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
    confirmed=false;
#endif
    report_storage_ready=pending_mark=delivered_mark=false;
    advertising_started=0;ticks=0;resets=0;fail_confirm=false;
    pending_line[0]=0;cached=(struct inst_record){0};
    read_kind=kind;read_update=update;
    et100_boot_poll(1);
}

static void test_app_announcements(void)
{
    char line[121];
    memset(read_hash,0x31,sizeof read_hash);
    reboot_health(BOOT_NONE,UPDATE_OK);
    assert(!et100_boot_line(line,sizeof line));
    reboot_health(BOOT_CONFIRMED,UPDATE_OK);
    assert(!et100_boot_line(line,sizeof line));
    reboot_health(BOOT_TRIAL,UPDATE_OK);
    assert(et100_boot_line(line,sizeof line) && strstr(line,";APP;") && strstr(line,";TRIAL;"));
    et100_boot_line_sent();
    assert(!result_reported(read_hash)); /* TRIAL APP must not hide a later rollback. */
    for (unsigned result=UPDATE_FAILED;result<=UPDATE_ABORTED;result++) {
        read_hash[31]++;
        reboot_health(BOOT_CONFIRMED,result);
        assert(et100_boot_line(line,sizeof line) && strstr(line,";APP;") &&
            strstr(line,result==UPDATE_FAILED?";ROLLED_BACK*":";ABORTED*"));
        assert(!result_reported(read_hash)); /* Dequeue is not transmission. */
        assert(!et100_boot_line(line,sizeof line));
        et100_boot_line_sent();assert(result_reported(read_hash));
        reboot_health(BOOT_CONFIRMED,result);
        assert(!et100_boot_line(line,sizeof line));
    }
    /* Same prefix is not the same transaction; failed persistence retries on reboot. */
    read_hash[31]++;fail_report_write=true;
    reboot_health(BOOT_CONFIRMED,UPDATE_FAILED);
    assert(et100_boot_line(line,sizeof line));et100_boot_line_sent();
    assert(!result_reported(read_hash));
    fail_report_write=false;
    reboot_health(BOOT_CONFIRMED,UPDATE_FAILED);
    assert(et100_boot_line(line,sizeof line));et100_boot_line_sent();
    assert(result_reported(read_hash));
    reboot_health(BOOT_CONFIRMED,UPDATE_FAILED);
    assert(!et100_boot_line(line,sizeof line));
    /* A reset between dequeue and send must not persist a result as delivered. */
    read_hash[31]++;
    reboot_health(BOOT_CONFIRMED,UPDATE_ABORTED);
    assert(et100_boot_line(line,sizeof line));
    reboot_health(BOOT_CONFIRMED,UPDATE_ABORTED);
    assert(et100_boot_line(line,sizeof line));et100_boot_line_sent();
    assert(result_reported(read_hash));
    /* Malformed stored value is not a matching hash. */
    assert(flashdb_set(ET100_FDB_INST_REPORTED_KEY,read_hash,8)==FDB_NO_ERR);
    reboot_health(BOOT_CONFIRMED,UPDATE_ABORTED);
    assert(et100_boot_line(line,sizeof line));et100_boot_line_sent();
    assert(result_reported(read_hash));
    puts("PASS S4: quiet boot, TRIAL, terminal once/full hash, send-before-mark, persistence retry");
}

int main(void)
{
    const uint32_t keys[]={ET100_FDB_GTIME_KEY,ET100_FDB_CID_KEY,ET100_FDB_AUTO_LIST_KEY,
                           ET100_FDB_SENSOR_PKG_KEY,ET100_FDB_BOOT_HEALTH_KEY,
                           ET100_FDB_REPORT_KEY,ET100_FDB_INST_REPORTED_KEY};
    for (unsigned i=0;i<7;i++) for (unsigned j=i+1;j<7;j++) assert(keys[i]!=keys[j]);
    /* Keys of records already on flash are frozen. */
    assert(ET100_FDB_GTIME_KEY==0x00030002U && ET100_FDB_CID_KEY==0x00030003U &&
           ET100_FDB_AUTO_LIST_KEY==0x00030004U && ET100_FDB_SENSOR_PKG_KEY==0x00030005U);

    uint8_t auto_list[100];
    for (unsigned i=0;i<sizeof auto_list;i++) auto_list[i]=(uint8_t)(i*7+1);
    flashdb_set(ET100_FDB_AUTO_LIST_KEY,auto_list,sizeof auto_list);
    uint8_t gtime[8]={'E','T',1,0,15,0,0xf0,0xff};
    flashdb_set(ET100_FDB_GTIME_KEY,gtime,sizeof gtime);

    et100_boot_poll(1);
    assert(trial && storage_ok);
    uint32_t marker=0;
    assert(flashdb_get_length(ET100_FDB_BOOT_HEALTH_KEY)==4);
    assert(flashdb_get(ET100_FDB_BOOT_HEALTH_KEY,(uint8_t *)&marker,4)==4 && marker==0x5a5a0007u);
    uint8_t check[100];
    assert(flashdb_get_length(ET100_FDB_AUTO_LIST_KEY)==sizeof auto_list);
    assert(flashdb_get(ET100_FDB_AUTO_LIST_KEY,check,sizeof check)==sizeof check);
    assert(!memcmp(check,auto_list,sizeof auto_list));
    assert(flashdb_get(ET100_FDB_GTIME_KEY,check,8)==8 && !memcmp(check,gtime,8));
    char announced[121];
    assert(et100_boot_line(announced,sizeof announced) && strstr(announced,";APP;") && strstr(announced,";TRIAL;"));
    et100_boot_line_sent();
    assert(!flashdb_get_length(ET100_FDB_INST_REPORTED_KEY));

    /* Probe passing still gates confirm with ADV + 30 s of healthy loop. */
    et100_boot_adv_started();
    for (ticks=1000;ticks<30000;ticks+=1000) et100_boot_poll(1);
    assert(!confirms);
    ticks=30000;et100_boot_poll(1);
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
    assert(confirms==1);
    assert(et100_boot_line(announced,sizeof announced) && strstr(announced,";CONFIRMED;"));
    assert(!flashdb_get_length(ET100_FDB_INST_REPORTED_KEY));
    et100_boot_line_sent();assert(result_reported(own_hash));
#else
    assert(confirms==0);
#endif
    struct inst_snapshot snapshot;assert(et100_boot_status(&snapshot));
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
    assert(snapshot.state==BOOT_CONFIRMED && snapshot.has_record);
#else
    assert(snapshot.state==BOOT_TRIAL);
#endif
    /* New trial, loop health repeatedly resets; absolute deadline must not move. */
    initialized=trial=trial_boot=timed_out=timeout_queued=false;
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
    confirmed=false;
#endif
    advertising_started=0;ticks=0;confirms=0;
    et100_boot_poll(1);
    for (ticks=3000;ticks<120000;ticks+=3000) et100_boot_poll(1);
    assert(!resets);
    ticks=120000;et100_boot_poll(1);assert(timed_out && !resets);
    char line[121];assert(et100_boot_line(line,sizeof line));
    assert(strstr(line,";TRIAL_TIMEOUT;") && strstr(line,";120;BLE*"));
    ticks=120001;et100_boot_poll(1);assert(resets==1 && cached.result==IR_TIMEOUT);
    assert(erase_state(BOOT_STATE_BASE,4096)==-1);
    assert(program_state(BOOT_STATE_BASE,"x",1)==-1);
    /* Deadline is enforced even before the next health poll latches timeout. */
    timed_out=false;boot.kind=BOOT_TRIAL;
    assert(trial_expired());
    assert(port.erase(BOOT_STATE_BASE,4096)==-1);
    assert(port.program(BOOT_STATE_BASE,"x",1)==-1);
    /* Confirm writes failing forever also expire, rather than feeding forever. */
    initialized=trial=trial_boot=timed_out=timeout_queued=false;
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
    confirmed=false;
#endif
    advertising_started=1;ticks=0;resets=0;fail_confirm=true;
    et100_boot_poll(1);
    for(ticks=1000;ticks<=120000;ticks+=1000)et100_boot_poll(1);
    assert(timed_out);
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
    assert(confirms>1);
#else
    assert(confirms==0);
#endif
    assert(et100_boot_line(line,sizeof line) && strstr(line,";120;WRITE*"));
    ticks=121000;et100_boot_poll(1);assert(resets);
    cached=(struct inst_record){0};boot.kind=BOOT_CONFIRMED;boot.last_update=UPDATE_ABORTED;
    assert(et100_boot_status(&snapshot)&&snapshot.has_record&&snapshot.record.result==IR_ABORTED&&
        snapshot.record.code==IC_RESULT_UNAVAILABLE);
    boot.last_update=UPDATE_FAILED;
    assert(et100_boot_status(&snapshot)&&snapshot.record.result==IR_ROLLED_BACK);
    read_kind=BOOT_PENDING;et100_boot_refresh();assert(et100_boot_status(&snapshot)&&snapshot.state==BOOT_PENDING);
    read_kind=BOOT_CONFIRMED;et100_boot_refresh();assert(et100_boot_status(&snapshot)&&snapshot.state==BOOT_CONFIRMED);
    read_kind=BOOT_TRIAL;
    for(unsigned storage=0;storage<=1;storage++) {
        initialized=trial=trial_boot=timed_out=timeout_queued=storage_ok=false;
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
        confirmed=false;
#endif
        advertising_started=1;resets=0;ticks=0;fail_confirm=false;pending_line[0]=0;
        et100_boot_poll((int)storage);
        for(ticks=3000;ticks<=120000;ticks+=3000)et100_boot_poll((int)storage);
        assert(timed_out && et100_boot_line(line,sizeof line));
        assert(strstr(line,storage?";120;LOOP*":";120;STORAGE*"));
        unsigned last_feeds=feeds;
        ticks=121000;et100_boot_poll((int)storage);assert(resets&&feeds==last_feeds);
    }
    test_app_announcements();
    assert(flashdb_get(ET100_FDB_AUTO_LIST_KEY,check,sizeof check)==sizeof check && !memcmp(check,auto_list,sizeof check));
    puts("boot health tests passed");
    return 0;
}
