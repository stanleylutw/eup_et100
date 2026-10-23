/* Execute production worker scheduling against a manager/SMP fixture, no hardware. */
#include <setjmp.h>
#include <stdio.h>
#include "FreeRTOS.h"
typedef uint32_t TickType_t;
#define __DMB() ((void)0)
#define taskYIELD() ((void)0)
#define portMAX_DELAY UINT32_MAX
#define pdTRUE 1
void xTaskNotifyGive(TaskHandle_t task);
uint32_t ulTaskNotifyTake(BaseType_t clear, uint32_t timeout);
void NVIC_SystemReset(void);
int flashdb_del(uint32_t key);
#include "../src/ota_worker.c"

static uint32_t ticks, cycles;
static int operation, processed;
static jmp_buf cycle_end;
uint32_t xTaskGetTickCount(void) { return ticks; }
void vTaskDelay(uint32_t n) { ticks += n; }
void xTaskNotifyGive(TaskHandle_t task) { (void)task; }
uint32_t ulTaskNotifyTake(BaseType_t clear, uint32_t timeout)
{
    (void)clear;(void)timeout;stack_low=NULL;
    if (cycles++) longjmp(cycle_end,1);
    return 1;
}
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned prio,TaskHandle_t *out)
{ (void)fn;(void)name;(void)stack;(void)arg;(void)prio;*out=(void *)1;return pdPASS; }
void NVIC_SystemReset(void) { assert(0); }
static int nor_init(void) { return 0; }
static struct nor_port nor={.init=nor_init};
struct nor_port *nor_qspi1_port(void) { return &nor; }
int nor_qspi1_authorize_writes(void) { return 0; }
void et100_sys_set(unsigned i,unsigned s) { (void)i;(void)s; }
int inst_record_read(struct nor_port *p,struct inst_record *r) { (void)p;(void)r;return 0; }
int inst_record_write(struct nor_port *p,const struct inst_record *r) { (void)p;(void)r;return 0; }
void et100_boot_refresh(void) {}
const struct internal_port *et100_boot_port(void) { return NULL; }
size_t flashdb_get_length(uint32_t k) { (void)k;return 0; }
size_t flashdb_get(uint32_t k,uint8_t *p,uint32_t n) { (void)k;(void)p;(void)n;return 0; }
int flashdb_set(uint32_t k,uint8_t *p,uint32_t n) { (void)k;(void)p;(void)n;return 0; }
int flashdb_del(uint32_t k) { (void)k;return 0; }
void sensor_store_init(struct sensor_store *s,const struct sensor_store_ops *o) { (void)s;(void)o; }
void ota_manager_init(struct ota_manager *m,const struct ota_env *e) { memset(m,0,sizeof *m);m->env=e; }
void ota_manager_smp_ops(struct ota_manager *m,struct smp_ops *o) { (void)m;memset(o,0,sizeof *o); }
bool ota_manager_sensor_supported(const struct ota_manager *m) { return m->env->sensor!=NULL; }
void ota_manager_sensor_status(struct ota_manager *m,struct sensor_store_status *s) { *s=m->sensor_status; }
void ota_manager_set_source(struct ota_manager *m,uint32_t s) { m->active_source=s; }
void ota_manager_set_relay(struct ota_manager *m,bool active) { m->relay=active; }
bool ota_manager_upload_open(const struct ota_manager *m) { return m->session; }
int ota_manager_sensor_delete(struct ota_manager *m) { m->session=false;return m->relay?SMP_EBUSY:0; }
int ota_manager_sensor_read(struct ota_manager *m,uint32_t off,void *p,size_t n)
{ (void)m;(void)off;(void)p;(void)n;return 0; }
size_t smp_server_process(const struct smp_ops *o,const uint8_t *p,size_t n,uint8_t *r,size_t cap,bool *reset)
{
    (void)o;(void)p;(void)n;(void)r;(void)cap;*reset=false;
    assert(smp_in_flight && !ota_worker_relay_begin());
    processed++;
    if (operation==1) { ota.session=false;ota.owner_ms=ticks; }
    if (operation==2) { ota.session=true;ota.owner_ms=ticks; }
    return 0;
}
static void process(void)
{ cycles=0;if (!setjmp(cycle_end)) worker_task(NULL); }
static void request(int op,enum ota_transport transport)
{ operation=op;assert(ota_worker_submit(transport,(uint8_t[]){0},1,0));assert(!ota_worker_relay_begin());process(); }
int main(void)
{
    ota_worker_send_fn send[OTA_TRANSPORT_COUNT]={0};ota_worker_start(send,true);
    ticks=100;request(1,OTA_TRANSPORT_SERIAL);
    assert(ota_worker_active() && ota_worker_relay_begin());ota_worker_relay_end();
    ticks=200;request(0,OTA_TRANSPORT_BLE);
    assert(ota_worker_relay_begin());ota_worker_relay_end();
    ticks=300;request(2,OTA_TRANSPORT_SERIAL);assert(!ota_worker_relay_begin());
    ticks=30299;request(0,OTA_TRANSPORT_BLE);assert(!ota_worker_relay_begin());
    ticks=30300;assert(ota_worker_active() && ota_worker_relay_begin());ota_worker_relay_end();
    /* Both transport slots must drain before handoff; in-flight checks run above. */
    ticks=40000;operation=1;
    assert(ota_worker_submit(OTA_TRANSPORT_SERIAL,(uint8_t[]){0},1,0));
    assert(ota_worker_submit(OTA_TRANSPORT_BLE,(uint8_t[]){0},1,0));
    assert(!ota_worker_relay_begin());int before=processed;process();assert(processed==before+2);
    assert(ota_worker_sensor_delete() && !ota_worker_relay_begin());process();
    assert(ota_worker_relay_begin() && !ota_worker_relay_begin());ota_worker_relay_end();
    ticks=UINT32_MAX-100;request(2,OTA_TRANSPORT_SERIAL);
    ticks=100;request(0,OTA_TRANSPORT_BLE);assert(!ota_worker_relay_begin());
    ticks=29899;assert(ota_worker_relay_begin());ota_worker_relay_end();
    puts("PASS worker handoff: same-tick finish, queries, open upload hold, queue/in-flight/delete/duplicate, wrap, unchanged activity");
}
