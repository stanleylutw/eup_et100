#include <assert.h>
#include <stdio.h>
#include "../src/smp_ble.c"

static uint32_t ticks, last_rx;
static unsigned activities, submitted, notifications, fail_notify, disconnect_on_send;
static uint8_t submitted_data[1024];
static size_t submitted_length;

uint32_t xTaskGetTickCount(void) { return ticks; }
void ota_worker_smp_activity(void) { activities++;last_rx=ticks; }
bool ota_worker_submit(enum ota_transport transport,const uint8_t *p,size_t n,uint32_t tag)
{
    assert(transport==OTA_TRANSPORT_BLE && tag==generation);
    memcpy(submitted_data,p,n);submitted_length=n;submitted++;return true;
}
uint8_t gatt_add_service(struct gatt_service *s)
{ assert(s->gatt_msg_handler==server_callback);return 2; }
ble_api_err_t gatt_notification(struct gatt_send_event *e)
{
    assert(e->conidx==3 && e->svc_id==2 && e->att_idx==VALUE);
    assert(e->data_len<=20);notifications++;
    return fail_notify?BLE_API_ERR_NO_AVAILABLE_RESOURCE:BLE_API_ERR_NO_ERR;
}
void vTaskDelay(uint32_t delay)
{
    ticks+=delay;
    if (disconnect_on_send) {
        struct gap_event event={.type=GAP_EVT_DISCONNECT};event.param.disconnect.conidx=3;
        smp_ble_gap(&event);return;
    }
    if (outstanding) {
        struct gatt_msg msg={.msg_evt=GATTC_MSG_CMP_EVT,.conn_idx=3};
        msg.param.gatt_op_cmp.operation=GATT_OP_NOTIFY;server_callback(&msg);
    }
}
static void write_value(uint8_t *p,size_t n)
{
    struct gatt_msg msg={.msg_evt=GATTS_MSG_WRITE_REQ,.conn_idx=3,.att_idx=VALUE};
    msg.param.gatt_data.p_msg_data=p;msg.param.gatt_data.msg_len=(uint16_t)n;
    server_callback(&msg);
}
static void connect_peer(void)
{
    struct gap_event event={.type=GAP_EVT_SLAVE_CONNECT};event.param.connect.conidx=3;
    smp_ble_gap(&event);subscribed=1;
}
int main(void)
{
    uint8_t packet[60]={2,0,0,52};
    smp_ble_add_service();connect_peer();
    ticks=100;write_value(packet,20);
    assert(activities==1 && last_rx==100 && !submitted && smp_ble_receiving());
    ticks=2100;assert(smp_ble_receiving());ticks=2101;assert(!smp_ble_receiving());
    /* Every fragment records RX, not just completed requests. */
    ticks=3000;write_value(packet,20);ticks=3010;write_value(packet+20,40);
    assert(activities==3 && last_rx==3010 && submitted==1 && !smp_ble_receiving());
    assert(submitted_length==60 && !memcmp(submitted_data,packet,60));
    write_value(packet,20);assert(smp_ble_receiving());
    struct gap_event end={.type=GAP_EVT_DISCONNECT};end.param.disconnect.conidx=3;
    smp_ble_gap(&end);assert(!smp_ble_receiving());
    connect_peer();ticks=UINT32_MAX-100;write_value(packet,20);
    ticks=1899;assert(smp_ble_receiving());ticks=1900;assert(!smp_ble_receiving());
    assert(!smp_ble_send(packet,sizeof packet,generation) && notifications==3 && !outstanding);
    fail_notify=1;assert(smp_ble_send(packet,sizeof packet,generation)==-1);
    fail_notify=0;disconnect_on_send=1;assert(smp_ble_send(packet,sizeof packet,generation)==-1);
    assert(!smp_ble_receiving());
    puts("PASS BLE SMP activity: first/every fragment, periodic expiry, disconnect, wrap, segmented reply/error/abort");
}
