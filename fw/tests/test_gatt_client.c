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
{ if(scenario!=5) { struct gap_event e={.type=GAP_EVT_SCAN_END};et100_scan_status=0x100;et100_gatt_gap(&e); }
  return 0; }
ble_api_err_t gap_conn_start(struct gap_conn_param *p)
{ assert(p->peer_addr.addr.addr[0]==0xff);if(scenario!=3 && scenario!=4 && scenario!=6)simulate_connect();return 0; }
ble_api_err_t gap_conn_stop(void)
{ stop_calls++;if(scenario==3)simulate_connect();
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
ble_api_err_t gatt_discovery_peer_svc(uint8_t id,uint8_t index,struct gatt_uuid *uuid)
{ (void)id;(void)index;assert(uuid_is(uuid,0));gatt_svc_report_t svc={.start_hdl=1,.end_hdl=10,.uuid=*uuid};
  struct gatt_msg m={.msg_evt=GATTC_MSG_SVC_REPORT,.conn_idx=2};m.param.gatt_data.msg_len=sizeof(svc);
  m.param.gatt_data.p_msg_data=(void *)&svc;client_callback(&m);complete(GATT_OP_PEER_SVC_DISC_END);return 0; }
ble_api_err_t gatt_discovery_peer_char(uint8_t id,uint8_t index,uint16_t start,uint16_t end)
{ (void)id;(void)index;assert(start==1 && end==10);
  for(unsigned i=1;i<=2;i++) { gatt_char_report_t c={.char_hdl=(uint16_t)(i*2),.val_hdl=(uint16_t)(i*2+1),
      .prop=i==1?GATT_PROP_WRITE_CMD:GATT_PROP_NOTI,.uuid=service_uuid};c.uuid.p_uuid[12]=(uint8_t)i;
    struct gatt_msg m={.msg_evt=GATTC_MSG_CHAR_REPORT,.conn_idx=2};m.param.gatt_data.msg_len=sizeof(c);
    m.param.gatt_data.p_msg_data=(void *)&c;client_callback(&m); }
  complete(GATT_OP_PEER_CHAR_DISC_END);return 0; }
ble_api_err_t gatt_discovery_peer_desc(uint8_t id,uint8_t index,uint16_t start,uint16_t end)
{ (void)id;(void)index;assert(start==6 && end==10);gatt_desc_report_t d={.desc_hdl=6,.uuid={2,{2,0x29}}};
  struct gatt_msg m={.msg_evt=GATTC_MSG_CHAR_DESC_REPORT,.conn_idx=2};m.param.gatt_data.msg_len=sizeof(d);
  m.param.gatt_data.p_msg_data=(void *)&d;client_callback(&m);complete(GATT_OP_PEER_DESC_DISC_END);return 0; }
ble_api_err_t gatt_cilent_write_req_with_handle(struct gatt_client_write_param_handle *p)
{ assert(p->handle==6 && p->data_len==2 && p->p_data[0]==1);complete(GATT_OP_WRITE_REQ);return 0; }
ble_api_err_t gatt_cilent_write_cmd_with_handle(struct gatt_client_write_param_handle *p)
{
    assert(p->handle==3 && p->data_len>=7);
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
int main(void)
{
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
    puts("PASS: public AES vector, real SDK types, discovery/CCCD, auth pass/fail, read/config,");
    puts("      cancel/late-connect, zero-status drain, scan restore, auto-list reload/failure, ordered/stale output");
    return 0;
}
