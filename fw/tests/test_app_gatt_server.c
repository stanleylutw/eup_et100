#include <assert.h>
#include <stdio.h>
#include "../src/app_gatt_server.c"
#include "../src/evb_serial_ble.c"

static uint32_t ticks;
static unsigned notifications,fail_send,immediate_completion,rejected,adv_starts,scan_starts;
static uint16_t sent_length;
static uint8_t sent[ET100_MANAGER_SELECTED_MAX];
static gap_callback_func_t gap_handler;
static uint8_t peer_mac[6]={0x1f,0x30,0xe9,0x30,0xdb,0xfa};
static const uint8_t raw[]={2,1,6,17,0xff,0x39,0x06,0x10,3,6,14,0x11,0,0x20,0,0,1,2,3,4,5};
uint32_t xTaskGetTickCount(void) {return ticks;}
static void complete(uint8_t status)
{
    struct gatt_msg msg={.msg_evt=GATTC_MSG_CMP_EVT,.conn_idx=3};
    msg.param.gatt_op_cmp.operation=GATT_OP_NOTIFY;msg.param.gatt_op_cmp.status=status;
    server_callback(&msg);
}
uint8_t gatt_add_service(struct gatt_service *service)
{
    assert(service->att_nb==10 && service->gatt_msg_handler==server_callback);
    assert(service->p_att_tb[LIST].max_size==132 && service->p_att_tb[DATA].max_size==54);
    assert(!memcmp(service->p_att_tb[STATUS].uuid.p_uuid,(uint8_t[])ET100_APP_UUID(2),16));
    return 4;
}
ble_api_err_t gatt_notification(struct gatt_send_event *event)
{
    assert(event->conidx==3 && event->svc_id==4 && event->att_idx==DATA && in_flight);
    notifications++;sent_length=event->data_len;memcpy(sent,event->p_data,sent_length);
    if(immediate_completion)complete(0);
    return fail_send?BLE_API_ERR_NO_AVAILABLE_RESOURCE:BLE_API_ERR_NO_ERR;
}
ble_api_err_t gap_disconnect(uint8_t index) {assert(index==7);rejected++;return 0;}
ble_api_err_t gap_set_cb_func(gap_callback_func_t cb) {gap_handler=cb;return 0;}
ble_api_err_t gap_security_param_init(struct gap_security_param *s) {(void)s;return 0;}
void et100_gatt_add_profile(void) {assert(service_id==4);}
void ota_gatt_add_service(void) {}
void et100_gatt_gap(struct gap_event *event) {(void)event;}
unsigned et100_gatt_state(void) {return 0;}
void et100_scan_event(struct gap_event *event) {(void)event;}
void et100_scan_start(void) {scan_starts++;}
void et100_boot_adv_started(void) {}
adv_handle gap_adv_create(void) {return (void *)1;}
ble_api_err_t gap_adv_set_param(adv_handle handle,struct gap_adv_param *p)
{assert(handle==(void *)1 && p->adv_intv_min==160);return 0;}
ble_api_err_t gap_adv_set_adv_data(adv_handle handle,const uint8_t *p,uint16_t n)
{
    (void)handle;assert(n<=28 && p[0]==n-1 && p[1]==GAP_ADVTYPE_LOCAL_NAME_COMPLETE);
    assert(n==strlen(ET100_APP_NAME)+2 && !memcmp(p+2,ET100_APP_NAME,n-2));return 0;
}
ble_api_err_t gap_adv_set_scan_rsp(adv_handle handle,const uint8_t *p,uint16_t n)
{(void)handle;assert(n==18 && p[0]==17 && p[1]==7 && !memcmp(p+2,manager_uuid,16));return 0;}
ble_api_err_t gap_adv_start(adv_handle handle,uint16_t duration,uint8_t max)
{assert(handle==(void *)1 && !duration && !max);adv_starts++;return 0;}
ble_api_err_t gatt_mtu_exchange_req(uint8_t id,uint8_t link,uint16_t requested)
{assert(id==4 && (link==3 || link==7) && requested==247);return 0;}
ble_api_err_t gap_name_req_rsp(uint8_t link,uint16_t token,uint16_t n,uint8_t *name)
{(void)link;(void)token;assert(n==strlen(ET100_APP_NAME) && !memcmp(name,ET100_APP_NAME,n));return 0;}
ble_api_err_t gap_appearance_req_rsp(uint8_t link,uint16_t token,uint16_t appearance)
{(void)link;(void)token;(void)appearance;return 0;}
ble_api_err_t gap_param_update_rsp(struct gap_link_param_update_rsp *p) {assert(p->accept);return 0;}
static void gap_event(enum gap_event_type type,uint8_t index)
{
    struct gap_event event={.type=type};event.param.connect.conidx=index;gap_handler(&event);
}
static void receive(void)
{
    struct et100_reporter reporter;uint16_t cids[4]={0x0639,0,0,0};struct et100_adv_details details;
    et100_reporter_init(&reporter,cids,15,ticks);
    assert(et100_reporter_receive_details(&reporter,peer_mac,1,-50,raw,sizeof(raw),ticks,&details));
    et100_app_server_receive(peer_mac,1,-50,0,raw,sizeof(raw),&details,ticks);
}
static uint16_t read_value(unsigned att,uint16_t offset,uint8_t *p,uint16_t capacity)
{
    struct gatt_msg msg={.msg_evt=GATTS_MSG_READ_REQ,.conn_idx=3,.att_idx=(uint16_t)att};
    msg.param.gatt_data.p_msg_data=p;msg.param.gatt_data.msg_len=capacity;msg.param.gatt_data.offset=offset;
    return server_callback(&msg);
}
static void write_value(unsigned att,uint8_t *p,uint16_t n,uint16_t offset,uint8_t link)
{
    struct gatt_msg msg={.msg_evt=GATTS_MSG_WRITE_REQ,.conn_idx=link,.att_idx=(uint16_t)att};
    msg.param.gatt_data.p_msg_data=p;msg.param.gatt_data.msg_len=n;msg.param.gatt_data.offset=offset;
    server_callback(&msg);
}
int main(void)
{
    app_ble_init();gap_event(GATT_EVT_PROFILE_ADDED,0);assert(adv_starts==1);
    gap_event(GATT_EVT_PROFILE_ADDED,0);assert(adv_starts==1);
    gap_event(GAP_EVT_ADV_START,0);assert(scan_starts==1);
    gap_event(GAP_EVT_NAME_REQ,0);gap_event(GAP_EVT_SLAVE_CONNECT,3);
    uint8_t value[160]={0};assert(read_value(STATUS,0,value,16)==16 && value[3]==0xff);
    write_value(CCC,(uint8_t[]){1,0},2,0,3);ticks=100;et100_app_server_poll();
    assert(notifications==1 && sent_length==6 && sent[1]==0xff);complete(0);
    receive();write_value(SELECT,(uint8_t[]){1,1,0,0},4,0,3);
    ticks=200;et100_app_server_poll();assert(notifications==1); /* MTU 23: no truncation. */
    struct gap_event mtu_event={.type=GATT_EVT_MTU};mtu_event.param.mtu_ind.conidx=3;
    mtu_event.param.mtu_ind.mtu=247;gap_handler(&mtu_event);et100_app_server_poll();
    assert(notifications==2 && sent_length==23+sizeof(raw) && !memcmp(sent+23,raw,sizeof(raw)));
    ticks=300;receive();et100_app_server_poll();assert(notifications==2 && in_flight);
    complete(0);et100_app_server_poll();assert(notifications==3);complete(0);
    assert(read_value(LIST,0,value,7)==7 && value[0]==5 && value[1]==1);
    ticks=6000; /* Read-blob must retain the first fragment's snapshot. */
    assert(read_value(LIST,7,value+7,125)==13 && !memcmp(value+6,peer_mac,6));
    assert(!read_value(LIST,132,value,1));
    assert(read_value(DATA,0,value,54)==6 && value[1]==0xff);
    assert(!read_value(LIST,7,value,20)); /* Different attribute invalidates snapshot. */
    unsigned bad=et100_app_invalid_writes;
    write_value(SELECT,(uint8_t[]){1,1,0,0},4,1,3);
    write_value(CCC,(uint8_t[]){2,0},2,0,3);assert(et100_app_invalid_writes==bad+2 && subscribed);
    write_value(CCC,(uint8_t[]){0,0},2,0,9);assert(subscribed); /* Sensor link ignored. */
    ticks=6100;receive();write_value(SELECT,(uint8_t[]){1,1,0,0},4,0,3);
    fail_send=1;et100_app_server_poll();assert(!in_flight && force_notify && et100_app_notify_errors);
    unsigned count=notifications;et100_app_server_poll();assert(notifications==count);
    ticks+=100;fail_send=0;immediate_completion=1;et100_app_server_poll();assert(!in_flight);
    gap_event(GAP_EVT_SLAVE_CONNECT,7);assert(rejected==1 && app_link==3);
    struct gap_event lost={.type=GAP_EVT_DISCONNECT};lost.param.disconnect.conidx=2;
    lost.param.disconnect.local_role=LOCAL_ROLE_MASTER;gap_handler(&lost);
    assert(app_link==3 && adv_starts==1);
    lost.param.disconnect.conidx=7;lost.param.disconnect.local_role=LOCAL_ROLE_SLAVE;gap_handler(&lost);
    assert(app_link==3 && adv_starts==1); /* Rejected extra App must not restart advertising. */
    lost.param.disconnect.conidx=3;gap_handler(&lost);assert(!et100_app_server_active() && adv_starts==2);
    assert(!read_value(STATUS,0,value,16));
    gap_event(GAP_EVT_SLAVE_CONNECT,3);assert(!subscribed && mtu==23 && !in_flight);
    et100_app_server_poll();assert(!in_flight);
    assert(read_value(CCC,0,value,2)==2 && !value[0]);
    puts("PASS: App SDK callbacks, UUID/name AD lengths, bounded read/blob, CCCD/MTU, notify completion/retry, dual-role isolation/re-advertising");
    return 0;
}
