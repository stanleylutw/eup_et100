/* Cooperative callback simulation; production module and SDK API types are real. */
#include <setjmp.h>
#include <assert.h>
#include <tinycrypt/aes.h>
#include "../src/gatt_client.c"

static uint32_t ticks;
static uintptr_t current_task = 1;
static char captured[20000];
static size_t captured_size;
static struct queued_output queue_storage[16];
static unsigned queue_head, queue_tail, queue_count;
static uint8_t durable_auto[100];
static size_t durable_size;
static int db_fail, scenario, ready_step, restored, auth_writes, stop_calls;
static unsigned sys_ble_status;
void et100_sys_set(unsigned index, unsigned status) { assert(index==SYS_BLE);sys_ble_status=status; }
static jmp_buf worker_exit;
volatile unsigned et100_scan_status;

uint32_t xTaskGetTickCount(void) { return ticks; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)current_task; }
BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack,
                       void *arg, unsigned priority, TaskHandle_t *handle)
{ (void)task;(void)name;(void)stack;(void)arg;(void)priority;(void)handle; return pdPASS; }
QueueHandle_t xQueueCreate(unsigned n,unsigned size)
{ assert(n==16 && size==sizeof(struct queued_output)); queue_head=queue_tail=queue_count=0; return queue_storage; }
BaseType_t xQueueSend(QueueHandle_t q,const void *data,uint32_t timeout)
{ (void)q;(void)timeout; if(queue_count==16)return 0; memcpy(&queue_storage[queue_head],data,sizeof(struct queued_output));
  queue_head=(queue_head+1)%16;queue_count++;return pdPASS; }
BaseType_t xQueueReceive(QueueHandle_t q,void *data,uint32_t timeout)
{ (void)q;(void)timeout;if(!queue_count)return 0;memcpy(data,&queue_storage[queue_tail],sizeof(struct queued_output));
  queue_tail=(queue_tail+1)%16;queue_count--;return pdPASS; }
unsigned uxQueueMessagesWaiting(QueueHandle_t q) { (void)q;return queue_count; }
static void capture(const char *data,size_t n,void *ctx)
{ (void)ctx;assert(captured_size+n<sizeof(captured));memcpy(captured+captured_size,data,n);
  captured_size+=n;captured[captured_size]=0; }
static void drain_output(void)
{ uintptr_t old=current_task;current_task=1;while(queue_count)et100_gatt_poll(capture,NULL);current_task=old; }
void et100_scan_start(void) { et100_scan_status=0;restored++; }
size_t flashdb_get_length(uint32_t key) { assert(key==AUTO_KEY);return durable_size; }
size_t flashdb_get(uint32_t key,uint8_t *data,uint32_t length)
{ assert(key==AUTO_KEY);if(length<durable_size)return 0;memcpy(data,durable_auto,durable_size);return durable_size; }
int flashdb_set(uint32_t key,uint8_t *data,uint32_t length)
{ assert(key==AUTO_KEY && length==100);if(db_fail)return -1;memcpy(durable_auto,data,length);durable_size=length;return 0; }

static void simulate_connect(void)
{ struct gap_event e={.type=GAP_EVT_MASTER_CONNECT};e.param.connect.conidx=2;
  for(unsigned i=0;i<6;i++)e.param.connect.peer_addr.addr.addr[i]=target[5-i];et100_gatt_gap(&e); }
void vTaskDelay(uint32_t delay)
{
    ticks+=delay;assert(ticks<60000);drain_output();
    if(state==IDLE)longjmp(worker_exit,1);
    if(state==WAIT_ADV) {
        if(scenario==2)cancel_requested=1;
        else {
            struct gap_evt_adv_report ad={0};ad.evt_type=GAP_SCAN_EVT_CONN_UNDIR;
            for(unsigned i=0;i<6;i++)ad.addr.addr.addr[i]=target[5-i];
            struct gap_event e={.type=GAP_EVT_ADV_REPORT};e.param.adv_rpt=&ad;et100_gatt_gap(&e);
        }
    }
    if(state==CONNECTING && (scenario==3 || scenario==4 || scenario==5 || scenario==6))cancel_requested=1;
    if(state==DRAIN && scenario==5 && scan_stop_pending) {
        assert(restored==0 && et100_gatt_state()==1);
        if(ticks>2000) { struct gap_event e={.type=GAP_EVT_SCAN_END};
            et100_scan_status=0x100;et100_gatt_gap(&e); }
    }
    if(state==DRAIN && scenario==4) {
        assert(et100_gatt_state()==1);
        if(ticks>6500) { struct gap_event e={.type=GAP_EVT_CONN_END};e.param.conn_end_status=0;et100_gatt_gap(&e); }
        if(ticks>7000 && initiating)simulate_connect();
    }
    if(state==DRAIN && scenario==11 && ticks>26000) {
        struct gap_event e={.type=GAP_EVT_CONN_END};e.param.conn_end_status=0x16;et100_gatt_gap(&e);
    }
    if(state==READY) {
        uintptr_t old=current_task;current_task=1;
        if(ready_step==0) { char p[]="AABBCCDDEEFF;7F";assert(et100_gatt_uart("GSENSORCMD",p,"PING"));ready_step++; }
        else if(ready_step==1 && !request_pending) { char p[]="AABBCCDDEEFF;01;03";
            assert(et100_gatt_uart("SSENSORCFG",p,"CONF"));ready_step++; }
        else if(ready_step==2 && !request_pending) {
            char p[]="0;";assert(et100_gatt_uart("STYPESWITCH",p,"STOP"));ready_step++;
        }
        current_task=old;
    }
}
ble_api_err_t gap_scan_stop(void)
{ if(scenario!=5 && scenario!=9) { struct gap_event e={.type=GAP_EVT_SCAN_END};et100_scan_status=0x100;et100_gatt_gap(&e); }
  return 0; }
ble_api_err_t gap_conn_start(struct gap_conn_param *p)
{ assert(p->peer_addr.addr.addr[0]==0xff);if(scenario!=3 && scenario!=4 && scenario!=6 && scenario!=7 && scenario!=8)simulate_connect();return 0; }
ble_api_err_t gap_conn_stop(void)
{ stop_calls++;if(scenario==3)simulate_connect();
  if(scenario==7)return BLE_API_ERR_UNKNOWN;
  if(scenario==8) { struct gap_event e={.type=GAP_EVT_CONN_END};e.param.conn_end_status=0x16;et100_gatt_gap(&e); }
  if(scenario==6) { if(stop_calls<3)return BLE_API_ERR_UNKNOWN;
    struct gap_event e={.type=GAP_EVT_CONN_END};e.param.conn_end_status=0x16;et100_gatt_gap(&e); }
  return 0; }
ble_api_err_t gap_disconnect(uint8_t index)
{ assert(index==2);struct gap_event e={.type=GAP_EVT_DISCONNECT};e.param.disconnect.conidx=index;et100_gatt_gap(&e);return 0; }
uint8_t gatt_add_client(struct gatt_client *c) { assert(c->gatt_msg_handler==client_callback);return 1; }
ble_api_err_t gatt_mtu_exchange_req(uint8_t id,uint8_t index,uint16_t size)
{ (void)id;assert(index==2 && size==247);struct gap_event e={.type=GATT_EVT_MTU};
  e.param.mtu_ind.conidx=index;e.param.mtu_ind.mtu=247;et100_gatt_gap(&e);return 0; }
static void complete(uint8_t operation)
{ struct gatt_msg m={.msg_evt=GATTC_MSG_CMP_EVT,.conn_idx=2};m.param.gatt_op_cmp.operation=operation;client_callback(&m); }
static int smp_absent, smp_writes, cmd_writes[256];
static uint8_t smp_got[64];
static size_t smp_got_len;
ble_api_err_t gatt_discovery_peer_svc(uint8_t id,uint8_t index,struct gatt_uuid *uuid)
{ (void)id;(void)index;
  if(uuid->size==16 && !memcmp(uuid->p_uuid,smp_service_uuid,16)) {
    if(!smp_absent) { gatt_svc_report_t svc={.start_hdl=20,.end_hdl=30,.uuid=*uuid};
      struct gatt_msg m={.msg_evt=GATTC_MSG_SVC_REPORT,.conn_idx=2};m.param.gatt_data.msg_len=sizeof(svc);
      m.param.gatt_data.p_msg_data=(void *)&svc;client_callback(&m); }
    complete(GATT_OP_PEER_SVC_DISC_END);return 0; }
  assert(uuid_is(uuid,0));gatt_svc_report_t svc={.start_hdl=1,.end_hdl=10,.uuid=*uuid};
  struct gatt_msg m={.msg_evt=GATTC_MSG_SVC_REPORT,.conn_idx=2};m.param.gatt_data.msg_len=sizeof(svc);
  m.param.gatt_data.p_msg_data=(void *)&svc;client_callback(&m);complete(GATT_OP_PEER_SVC_DISC_END);return 0; }
ble_api_err_t gatt_discovery_peer_char(uint8_t id,uint8_t index,uint16_t start,uint16_t end)
{ (void)id;(void)index;
  if(start==20) { assert(end==30);
    gatt_char_report_t c[2]={{.char_hdl=21,.val_hdl=22,.prop=GATT_PROP_WRITE_CMD|GATT_PROP_NOTI,.uuid={16,{0}}},
                             {.char_hdl=24,.val_hdl=25,.prop=GATT_PROP_NOTI,.uuid={2,{1,2}}}};
    memcpy(c[0].uuid.p_uuid,smp_char_uuid,16);
    for(unsigned i=0;i<2;i++) { struct gatt_msg m={.msg_evt=GATTC_MSG_CHAR_REPORT,.conn_idx=2};
      m.param.gatt_data.msg_len=sizeof(c[i]);m.param.gatt_data.p_msg_data=(void *)&c[i];client_callback(&m); }
    complete(GATT_OP_PEER_CHAR_DISC_END);return 0; }
  assert(start==1 && end==10);
  for(unsigned i=1;i<=2;i++) { gatt_char_report_t c={.char_hdl=(uint16_t)(i*2),.val_hdl=(uint16_t)(i*2+1),
      .prop=i==1?GATT_PROP_WRITE_CMD:GATT_PROP_NOTI,.uuid=service_uuid};c.uuid.p_uuid[12]=(uint8_t)i;
    struct gatt_msg m={.msg_evt=GATTC_MSG_CHAR_REPORT,.conn_idx=2};m.param.gatt_data.msg_len=sizeof(c);
    m.param.gatt_data.p_msg_data=(void *)&c;client_callback(&m); }
  complete(GATT_OP_PEER_CHAR_DISC_END);return 0; }
ble_api_err_t gatt_discovery_peer_desc(uint8_t id,uint8_t index,uint16_t start,uint16_t end)
{ (void)id;(void)index;
  if(start==23) { assert(end==23);gatt_desc_report_t d={.desc_hdl=23,.uuid={2,{2,0x29}}};
    struct gatt_msg m={.msg_evt=GATTC_MSG_CHAR_DESC_REPORT,.conn_idx=2};m.param.gatt_data.msg_len=sizeof(d);
    m.param.gatt_data.p_msg_data=(void *)&d;client_callback(&m);complete(GATT_OP_PEER_DESC_DISC_END);return 0; }
  assert(start==6 && end==10);gatt_desc_report_t d={.desc_hdl=6,.uuid={2,{2,0x29}}};
  struct gatt_msg m={.msg_evt=GATTC_MSG_CHAR_DESC_REPORT,.conn_idx=2};m.param.gatt_data.msg_len=sizeof(d);
  m.param.gatt_data.p_msg_data=(void *)&d;client_callback(&m);complete(GATT_OP_PEER_DESC_DISC_END);return 0; }
ble_api_err_t gatt_cilent_write_req_with_handle(struct gatt_client_write_param_handle *p)
{ assert((p->handle==6 || p->handle==23) && p->data_len==2 && p->p_data[0]==1);complete(GATT_OP_WRITE_REQ);return 0; }
ble_api_err_t gatt_cilent_write_cmd_with_handle(struct gatt_client_write_param_handle *p)
{
    if(p->handle==22) {       /* SMP: echo the write back as one notification */
        smp_writes++;struct gatt_msg m={.msg_evt=GATTC_MSG_NTF_REQ,.conn_idx=2,.handle=22};
        m.param.gatt_data.p_msg_data=p->p_data;m.param.gatt_data.msg_len=p->data_len;client_callback(&m);return 0;
    }
    assert(p->handle==3 && p->data_len>=7);
    cmd_writes[p->p_data[3]]++;
    uint8_t cmd=p->p_data[3], answer[64]={0xe9,0x55,1,(uint8_t)(cmd|0x80),p->p_data[4],0,0,1,0};
    if(cmd==0x10) { answer[8]=18;answer[9]=1;answer[10]=16; }
    if(cmd==0x11) { auth_writes++;if(scenario==1)answer[5]=1; }
    if(cmd==0x01) { answer[8]=5;answer[9]=3;answer[10]=3;memcpy(answer+11,"1.0",3); }
    if(cmd==0x12)assert(p->p_data[5]==3 && p->p_data[6]==1 && p->p_data[7]==1 && p->p_data[8]==3);
    size_t n=10+answer[8];for(size_t i=0;i<n-1;i++)answer[n-1]+=answer[i];
    struct gatt_msg m={.msg_evt=GATTC_MSG_NTF_REQ,.conn_idx=2,.handle=5};
    m.param.gatt_data.p_msg_data=answer;m.param.gatt_data.msg_len=(uint16_t)n;client_callback(&m);return 0;
}
static void reset_test(int mode)
{
    state=IDLE;connected=initiating=matched=cancel_requested=output_fault=0;
    reserved=reserved_cancel=unreserve_pending=session_running=0;exclusive=NULL;
    scan_resume_pending=scan_stop_pending=connection_end_seen=radio_ready=0;cooldown_valid=0;request_pending=0;
    scenario=mode;ready_step=restored=auth_writes=stop_calls=0;ticks=0;captured_size=0;captured[0]=0;
    current_task=1;db_fail=0;et100_gatt_init(1);et100_gatt_poll(capture,NULL);et100_gatt_add_profile();
}
static void run_session(int mode)
{
    reset_test(mode);char parameters[]="1;AABBCCDDEEFF";
    assert(et100_gatt_uart("STYPESWITCH",parameters,"OPEN"));
    assert(et100_gatt_state()==1);
    current_task=2;
    if(!setjmp(worker_exit))worker(NULL);
    current_task=1;drain_output();assert(state==IDLE && !connected && !initiating);
    assert(strstr(captured,"G1;8;DC;AABBCCDDEEFF;"));
}
/* ---- v0.8.9 relay hooks ---- */
static int stages[8], stage_count, run_calls, run_result, hook_calls, matched_calls;
static void stage_fn(enum et100_gatt_stage s,int result,void *ctx)
{ assert(ctx==&stage_count && stage_count<8);stages[stage_count++]=result?-1-(int)s:(int)s; }
static void matched_fn(void *ctx) { (void)ctx;matched_calls++; }
static void sink_fn(const uint8_t *d,size_t n) { assert(smp_got_len+n<=sizeof(smp_got));memcpy(smp_got+smp_got_len,d,n);smp_got_len+=n; }
static int run_fn(void *ctx)
{
    (void)ctx;run_calls++;smp_got_len=0;
    assert(state==READY && connected && exclusive);
    struct eup_gatt_metadata m;
    /* All eight metadata commands are read before run(); the cache answers for target only. */
    for(unsigned c=1;c<=8;c++) assert(cmd_writes[c]==1);
    assert(et100_gatt_metadata(target,&m)==0 && m.seen);
    assert(et100_gatt_metadata((uint8_t[]){1,2,3,4,5,6},&m)==-ENODATA);
    char info[]="AABBCCDDEEFF";assert(et100_gatt_uart("GSENSORINFO",info,"INFO"));
    char p[]="AABBCCDDEEFF;7F";assert(et100_gatt_uart("GSENSORCMD",p,"BUSY"));   /* not during a relay */
    int open=et100_gatt_smp_open(sink_fn);
    if(smp_absent) { assert(open==-ENOENT && !smp_handle);return -ENOENT; }
    assert(open==0 && smp_handle==22 && smp_ccc==23 && smp_char_end==23 && ccc_handle==6 && tx_handle==5);
    uint8_t frame[10]={2,0,0,2,0,1,9,1,0xa0,0xa0};
    assert(et100_gatt_smp_write(frame,sizeof(frame))==0 && smp_got_len==10 && !memcmp(smp_got,frame,10));
    volatile unsigned never=0;
    assert(et100_gatt_mtu()==247 && et100_gatt_wait(&never,30)==-ETIMEDOUT);
    return run_result;
}
static int hook_fn(void) { return hook_calls++==0; }
static int exclusive_run(const struct et100_gatt_exclusive_ops *ops)
{
    int result=-999;current_task=2;
    if(!setjmp(worker_exit)) result=et100_gatt_exclusive((uint8_t[]){0xaa,0xbb,0xcc,0xdd,0xee,0xff},ops);
    current_task=1;drain_output();return result;
}
static void exclusive_tests(void)
{
    struct et100_gatt_exclusive_ops ops={5000,matched_fn,stage_fn,run_fn,&stage_count};
    /* Must hold the reservation; a second reservation is refused. */
    reset_test(0);ready_step=3;
    assert(exclusive_run(&ops)==-EBUSY && et100_gatt_reserve()==0 && et100_gatt_reserve()==-EBUSY);
    stage_count=run_calls=matched_calls=0;smp_got_len=0;run_result=0;memset(cmd_writes,0,sizeof(cmd_writes));
    assert(exclusive_run(&ops)==0 && run_calls==1 && matched_calls==1 && state==IDLE && !connected && !exclusive);
    int order[]={ET100_GATT_CONNECTED,ET100_GATT_AUTHENTICATING,ET100_GATT_AUTHENTICATED,
                 ET100_GATT_METADATA_READING,ET100_GATT_METADATA_READY};
    assert(stage_count==5 && !memcmp(stages,order,sizeof(order)));
    assert(strstr(captured,"G1;8;CS;") && strstr(captured,"G1;8;DC;AABBCCDDEEFF;") && !strstr(captured,"G1;8;MR;"));
    assert(strstr(captured,"GSENSORCMD;0;no_session;BUSY") && !smp_handle && !smp_sink);
    assert(strstr(captured,"AT$PD=BLEGW$GSENSORINFO;0;BUSY;INFO\r\n"));
    /* run() result passes through; missing SMP service. */
    stage_count=0;run_result=-EIO;memset(cmd_writes,0,sizeof(cmd_writes));assert(exclusive_run(&ops)==-EIO);
    smp_absent=1;stage_count=0;run_result=0;memset(cmd_writes,0,sizeof(cmd_writes));assert(exclusive_run(&ops)==-ENOENT);smp_absent=0;
    /* While reserved: no manual or auto session, no STYPESWITCH. */
    captured_size=0;captured[0]=0;
    char open[]="1;AABBCCDDEEFF";assert(et100_gatt_uart("STYPESWITCH",open,"OPEN"));
    char stop[]="0";assert(et100_gatt_uart("STYPESWITCH",stop,"STOP"));drain_output();
    assert(state==IDLE && strstr(captured,"STYPESWITCH;0;1;AABBCCDDEEFF;OPEN") && strstr(captured,"STYPESWITCH;0;0;STOP"));
    char info[]="AABBCCDDEEFF";assert(et100_gatt_uart("GSENSORINFO",info,"WAIT"));drain_output();
    assert(strstr(captured,"AT$PD=BLEGW$GSENSORINFO;0;BUSY;WAIT\r\n"));
    memcpy(auto_macs[0],(uint8_t[]){0xaa,0xbb,0xcc,0xdd,0xee,0xff},6);
    struct gap_evt_adv_report ad={0};ad.evt_type=GAP_SCAN_EVT_CONN_UNDIR;
    for(unsigned i=0;i<6;i++)ad.addr.addr.addr[i]=auto_macs[0][5-i];
    struct gap_event e={.type=GAP_EVT_ADV_REPORT};e.param.adv_rpt=&ad;et100_gatt_gap(&e);
    assert(state==IDLE);
    /* A cancel while idle between relay sessions stops the next session. */
    et100_gatt_cancel();assert(!cancel_requested && reserved_cancel);
    stage_count=run_calls=0;assert(exclusive_run(&ops)==-ECANCELED && !run_calls && !stage_count);
    assert(exclusive_run(&ops)==-ECANCELED);
    et100_gatt_unreserve();assert(!reserved && !reserved_cancel);
    assert(et100_gatt_uart("GSENSORINFO",info,"FREE"));drain_output();
    assert(strstr(captured,"AT$PD=BLEGW$GSENSORINFO;1;AABBCCDDEEFF;"));
    ticks+=6000;et100_gatt_gap(&e);assert(state==WAIT_ADV);   /* after the 5 s cooldown */
    state=IDLE;memset(auto_macs,0,sizeof(auto_macs));
    /* Auth failure is reported to the owner and returned. */
    reset_test(1);ready_step=3;assert(et100_gatt_reserve()==0);stage_count=run_calls=0;
    assert(exclusive_run(&ops)==-EACCES && !run_calls && stage_count==3 && stages[2]==-1-(int)ET100_GATT_AUTHENTICATED);
    et100_gatt_unreserve();
    /* No advertisement within wait_ms. */
    reset_test(2);assert(et100_gatt_reserve()==0);
    assert(exclusive_run(&ops)==-ECANCELED);   /* scenario 2 cancels in WAIT_ADV */
    et100_gatt_unreserve();
    /* Idle hook runs before the scan restart; OP events are Type 8 with checksum. */
    reset_test(0);et100_gatt_set_idle_hook(hook_fn);hook_calls=0;
    current_task=2;if(!setjmp(worker_exit))worker(NULL);current_task=1;
    assert(hook_calls==2);et100_gatt_set_idle_hook(NULL);
    et100_gatt_op_event((uint8_t[]){0xaa,0xbb,0xcc,0xdd,0xee,0xff},"SOTA;STEP;WAIT_ADV;ACTIVE");drain_output();
    const char *body="G1;8;OP;AABBCCDDEEFF;SOTA;STEP;WAIT_ADV;ACTIVE";uint8_t sum=0;
    for(const char *q=body;*q;q++)sum^=(uint8_t)*q;
    char expected[96];snprintf(expected,sizeof(expected),"AT$PD=%s*%02X\r\n",body,sum);
    assert(strstr(captured,expected));
    puts("PASS: relay exclusive session: reserve, stage order, metadata, SMP discovery/notify, run result,");
    puts("      no manual/auto/STYPESWITCH while reserved, sticky cancel, auth failure, idle hook, OP event");
}

static void drain_tests(void)
{
    struct et100_gatt_exclusive_ops ops={5000,matched_fn,stage_fn,run_fn,&stage_count};
    reset_test(8);assert(et100_gatt_reserve()==0);
    assert(exclusive_run(&ops)==-ENOTCONN && state==IDLE);
    assert(strstr(captured,"connect_timeout") && !connected && !initiating);
    et100_gatt_unreserve();
    reset_test(7);assert(et100_gatt_reserve()==0);
    assert(exclusive_run(&ops)==-ENOTCONN && state==DRAIN && initiating);
    assert(ticks>=25000 && ticks<25200 && !session_running);
    assert(sys_ble_status==SYS_ERROR && drain_stop_rc==BLE_API_ERR_UNKNOWN);
    assert(strstr(captured,"disconnect_drain_timeout:i=1,c=0,s=0,stop="));
    assert(et100_gatt_reserve()==-EBUSY);
    et100_gatt_unreserve();assert(reserved && unreserve_pending);
    uint32_t reported_at=drain_report_ms;
    drain_poll();assert(drain_report_ms==reported_at);
    ticks+=10000;drain_poll();assert(drain_report_ms>reported_at);
    assert(et100_gatt_reserve()==-EBUSY);
    /* D9 deferred: successful zero-status action alone must retain the owner. */
    struct gap_event e={.type=GAP_EVT_CONN_END};e.param.conn_end_status=0;et100_gatt_gap(&e);
    assert(initiating && !drain_poll());
    /* A late connection is drained before another owner may start. */
    simulate_connect();assert(connected && state==DRAIN && et100_gatt_reserve()==-EBUSY);
    ticks+=1000;assert(!drain_poll());assert(!connected);
    assert(drain_poll() && state==IDLE && !reserved);
    reset_test(9);assert(et100_gatt_reserve()==0);
    assert(exclusive_run(&ops)==-ENOTCONN && state==DRAIN && scan_stop_pending);
    et100_gatt_unreserve();assert(reserved);
    e.type=GAP_EVT_SCAN_END;et100_gatt_gap(&e);assert(drain_poll() && !reserved);
    reset_test(7);assert(et100_gatt_reserve()==0);
    assert(exclusive_run(&ops)==-ENOTCONN && state==DRAIN);
    et100_gatt_unreserve();scenario=11;current_task=2;
    if(!setjmp(worker_exit))worker(NULL);
    current_task=1;assert(state==IDLE && !reserved && !initiating);
    /* Output backpressure cannot report IDLE until DC is queued. */
    reset_test(0);state=DRAIN;reserved=1;unreserve_pending=1;
    struct queued_output item={0};while(queue_count<16)assert(xQueueSend(output_queue,&item,0)==pdPASS);
    assert(!release_owner() && state==DRAIN && reserved);
    xQueueReceive(output_queue,&item,0);assert(release_owner() && state==IDLE && !reserved);
    puts("PASS bounded drain: 15s return, stop errors, deferred D9, late link, scan pending, queue pressure, retained reservation");
}

int main(void)
{
    drain_tests();
    /* Public NIST AES-128 known-answer vector, unrelated to project credentials. */
    uint8_t key[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    uint8_t plain[16]={0,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    uint8_t expected[16]={0x69,0xc4,0xe0,0xd8,0x6a,0x7b,4,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a}, cipher[16];
    struct tc_aes_key_sched_struct schedule;
    assert(tc_aes128_set_encrypt_key(&schedule,key)==1 && tc_aes_encrypt(cipher,plain,&schedule)==1);
    assert(!memcmp(cipher,expected,16));
    uint8_t packet[48];assert(eup_uuid_build_request(0x7f,1,NULL,0,packet,sizeof(packet))==7);
    struct eup_uuid_response parsed;assert(eup_uuid_parse_response(packet,7,&parsed)<0);
    run_session(0);assert(auth_writes==1 && strstr(captured,"G1;8;AP;"));
    assert(strstr(captured,"AT$PD=BLEGW$GSENSORCMD;1;AABBCCDDEEFF;7F;00;;PING"));
    assert(strstr(captured,"AT$PD=BLEGW$SSENSORCFG;1;AABBCCDDEEFF;01;03;CONF"));
    assert(restored>0);
    run_session(1);assert(auth_writes==1 && strstr(captured,"G1;8;AF;") && !strstr(captured,"G1;8;AP;"));
    run_session(2);assert(auth_writes==0 && !strstr(captured,"G1;8;AP;"));
    run_session(3);assert(auth_writes==0 && restored>0);
    run_session(4);assert(strstr(captured,"disconnect_drain_timeout") && restored>0);
    assert(stop_calls>=6);
    run_session(5);assert(restored>0 && !auth_writes);
    run_session(6);assert(stop_calls==3 && restored>0);
    reset_test(0);char auto_on[]="AABBCCDDEEFF;1";
    assert(et100_gatt_uart("SAUTOGATT",auto_on,"AUTO"));assert(auto_enabled((uint8_t[]){0xaa,0xbb,0xcc,0xdd,0xee,0xff}));
    memset(auto_macs,0,sizeof(auto_macs));et100_gatt_init(1);
    assert(auto_enabled((uint8_t[]){0xaa,0xbb,0xcc,0xdd,0xee,0xff}));
    reset_test(0);memcpy(target,(uint8_t[]){0xaa,0xbb,0xcc,0xdd,0xee,0xff},6);
    state=READY;connected=1;event("AP","");char cancel[]="0";
    assert(et100_gatt_uart("STYPESWITCH",cancel,"STOP"));drain_output();
    assert(!strstr(captured,"G1;8;AP;") && strstr(captured,"STYPESWITCH;1;0;STOP"));
    reset_test(0);state=READY;connected=1;
    char html_cancel[]="0;";
    assert(et100_gatt_uart("STYPESWITCH",html_cancel,"KKM8"));drain_output();
    assert(cancel_requested && state==READY && connected);
    assert(strstr(captured,"AT$PD=BLEGW$STYPESWITCH;1;0;KKM8\r\n"));
    const char *bad_modes[]={"0;;","0;garbage","1;","1;AABBCCDDEEFF;","1;;AABBCCDDEEFF","2",""};
    for(unsigned i=0;i<sizeof(bad_modes)/sizeof(bad_modes[0]);i++) {
        reset_test(0);char bad[40];snprintf(bad,sizeof(bad),"%s",bad_modes[i]);
        assert(et100_gatt_uart("STYPESWITCH",bad,"BAD0"));drain_output();
        assert(!cancel_requested && state==IDLE && strstr(captured,"STYPESWITCH;0;"));
    }
    reset_test(0);char html_connect[]="1;aabbccddeeff";
    assert(et100_gatt_uart("STYPESWITCH",html_connect,"HTML"));drain_output();
    assert(state==WAIT_ADV && strstr(captured,"STYPESWITCH;1;1:AABBCCDDEEFF;HTML\r\n"));
    reset_test(0);memcpy(target,(uint8_t[]){0xaa,0xbb,0xcc,0xdd,0xee,0xff},6);
    event("DC","");char reopen[]="1:AABBCCDDEEFF";
    assert(et100_gatt_uart("STYPESWITCH",reopen,"NEXT"));drain_output();
    assert(strstr(captured,"G1;8;DC;") < strstr(captured,"STYPESWITCH;1;1:AABBCCDDEEFF;NEXT"));
    reset_test(0);state=READY;connected=1;conidx=2;event("AP","");
    struct gap_event disconnected={.type=GAP_EVT_DISCONNECT};disconnected.param.disconnect.conidx=2;
    et100_gatt_gap(&disconnected);drain_output();assert(!strstr(captured,"G1;8;AP;"));
    reset_test(0);current_task=1;
    for(unsigned i=0;i<20;i++) { char empty[]="";et100_gatt_uart("GAUTOLIST",empty,"LOAD"); }
    assert(!captured_size && queue_count==16 && output_fault && cancel_requested);
    drain_output();
    reset_test(0);struct gap_event started={.type=GAP_EVT_ADV_START};et100_gatt_gap(&started);
    current_task=2;if(!setjmp(worker_exit))worker(NULL);current_task=1;
    assert(restored==1);
    db_fail=1;char auto_off[]="AABBCCDDEEFF;0";
    assert(et100_gatt_uart("SAUTOGATT",auto_off,"FAIL"));
    assert(auto_enabled((uint8_t[]){0xaa,0xbb,0xcc,0xdd,0xee,0xff}));
    exclusive_tests();
    puts("PASS: public AES vector, real SDK types, discovery/CCCD, auth pass/fail, read/config,");
    puts("      cancel/late-connect, zero-status drain, scan restore, auto-list reload/failure, ordered/stale output");
    return 0;
}
