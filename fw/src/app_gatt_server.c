#include "app_gatt_server.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gap_api.h"
#include "gatt_api.h"
#include <string.h>

enum { SERVICE, STATUS_DECL, STATUS, LIST_DECL, LIST, SELECT_DECL, SELECT,
       DATA_DECL, DATA, CCC, ATTRIBUTE_COUNT };
static const uint8_t manager_uuid[]=ET100_APP_UUID(1);
static const gatt_attribute_t attributes[]={
    [SERVICE]={{UUID_SIZE_2,UUID16_ARR(GATT_PRIMARY_SERVICE_UUID)},GATT_PROP_READ,16,(uint8_t *)manager_uuid},
    [STATUS_DECL]={{UUID_SIZE_2,UUID16_ARR(GATT_CHARACTER_UUID)},GATT_PROP_READ,0,NULL},
    [STATUS]={{16,ET100_APP_UUID(2)},GATT_PROP_READ,16,NULL},
    [LIST_DECL]={{UUID_SIZE_2,UUID16_ARR(GATT_CHARACTER_UUID)},GATT_PROP_READ,0,NULL},
    [LIST]={{16,ET100_APP_UUID(3)},GATT_PROP_READ,ET100_MANAGER_LIST_MAX,NULL},
    [SELECT_DECL]={{UUID_SIZE_2,UUID16_ARR(GATT_CHARACTER_UUID)},GATT_PROP_READ,0,NULL},
    [SELECT]={{16,ET100_APP_UUID(4)},GATT_PROP_WRITE_REQ,10,NULL},
    [DATA_DECL]={{UUID_SIZE_2,UUID16_ARR(GATT_CHARACTER_UUID)},GATT_PROP_READ,0,NULL},
    [DATA]={{16,ET100_APP_UUID(5)},GATT_PROP_READ|GATT_PROP_NOTI,ET100_MANAGER_SELECTED_MAX,NULL},
    [CCC]={{UUID_SIZE_2,UUID16_ARR(GATT_CLIENT_CHAR_CFG_UUID)},GATT_PROP_READ|GATT_PROP_WRITE_REQ,2,NULL},
};
static struct et100_manager manager={.selected=0xff};
static uint8_t service_id=0xff, app_link=0xff;
static uint16_t mtu=23;
static unsigned subscribed, in_flight, force_notify, generation;
static uint32_t last_attempt;
static uint8_t read_snapshot[ET100_MANAGER_LIST_MAX], notify_value[ET100_MANAGER_SELECTED_MAX];
static uint16_t read_length, read_attribute=0xffff, notify_length;
/* Diagnostic counters, not new UART fields. */
volatile unsigned et100_app_invalid_writes, et100_app_notify_errors;

static uint32_t now_ms(void) {return (uint32_t)xTaskGetTickCount()*portTICK_PERIOD_MS;}
unsigned et100_app_server_active(void) {return app_link!=0xff;}

void et100_app_server_clear(void)
{
    taskENTER_CRITICAL();
    uint32_t list=manager.list_sequence+1,event=manager.event_sequence+1;
    et100_manager_init(&manager);manager.list_sequence=list;manager.event_sequence=event;
    force_notify=1;
    taskEXIT_CRITICAL();
}
void et100_app_server_receive(const uint8_t mac[6], uint8_t type, int8_t rssi,
    uint8_t adv_type, const uint8_t *raw, size_t n, const struct et100_adv_details *details, uint32_t now)
{
    taskENTER_CRITICAL();
    et100_manager_update(&manager,mac,type,rssi,adv_type,raw,n,details,now);
    taskEXIT_CRITICAL();
}
static uint16_t server_callback(struct gatt_msg *msg)
{
    uint16_t result=0;
    taskENTER_CRITICAL();
    if(msg->conn_idx!=app_link || app_link==0xff) goto done;
    if(msg->msg_evt==GATTC_MSG_CMP_EVT) {
        if(msg->param.gatt_op_cmp.operation==GATT_OP_NOTIFY) {
            in_flight=0;
            if(msg->param.gatt_op_cmp.status) {et100_app_notify_errors++;force_notify=1;}
        }
    } else if(msg->msg_evt==GATTS_MSG_READ_REQ) {
        size_t offset=msg->param.gatt_data.offset,capacity=msg->param.gatt_data.msg_len;
        if(!msg->param.gatt_data.p_msg_data || !capacity) goto done;
        if(offset==0) {
            read_attribute=msg->att_idx;read_length=0;
            if(msg->att_idx==STATUS) read_length=(uint16_t)et100_manager_status(&manager,now_ms(),read_snapshot,sizeof(read_snapshot));
            if(msg->att_idx==LIST) read_length=(uint16_t)et100_manager_list(&manager,now_ms(),read_snapshot,sizeof(read_snapshot));
            if(msg->att_idx==DATA) read_length=(uint16_t)et100_manager_selected(&manager,now_ms(),read_snapshot,sizeof(read_snapshot));
            if(msg->att_idx==CCC) {read_snapshot[0]=(uint8_t)subscribed;read_snapshot[1]=0;read_length=2;}
        }
        if(read_attribute==msg->att_idx && offset<read_length) {
            size_t n=read_length-offset;if(n>capacity)n=capacity;
            memcpy(msg->param.gatt_data.p_msg_data,read_snapshot+offset,n);result=(uint16_t)n;
        }
    } else if(msg->msg_evt==GATTS_MSG_WRITE_REQ) {
        const uint8_t *p=msg->param.gatt_data.p_msg_data;
        size_t n=msg->param.gatt_data.msg_len;
        if(msg->param.gatt_data.offset || !p) {et100_app_invalid_writes++;goto done;}
        if(msg->att_idx==CCC && n==2 && p[1]==0 && p[0]<=1) {
            subscribed=p[0];force_notify=1;
        } else if(msg->att_idx==SELECT && et100_manager_select(&manager,now_ms(),p,n)) {
            force_notify=1;
        } else et100_app_invalid_writes++;
    }
done:
    taskEXIT_CRITICAL();
    return result;
}
uint8_t et100_app_server_add(void)
{
    et100_app_server_clear();
    struct gatt_service service={.p_att_tb=attributes,.att_nb=ATTRIBUTE_COUNT,.gatt_msg_handler=server_callback};
    service_id=gatt_add_service(&service);return service_id;
}
void et100_app_server_gap(struct gap_event *event)
{
    uint8_t reject=0xff;
    taskENTER_CRITICAL();
    if(event->type==GAP_EVT_SLAVE_CONNECT) {
        uint8_t link=event->param.connect.conidx;
        if(app_link!=0xff && app_link!=link) reject=link;
        else {
            app_link=link;subscribed=in_flight=0;mtu=23;generation++;
            read_attribute=0xffff;read_length=notify_length=0;force_notify=1;
        }
    }
    if(event->type==GAP_EVT_DISCONNECT && event->param.disconnect.conidx==app_link) {
        app_link=0xff;subscribed=in_flight=0;generation++;read_attribute=0xffff;read_length=0;
    }
    if(event->type==GATT_EVT_MTU && event->param.mtu_ind.conidx==app_link) {
        mtu=event->param.mtu_ind.mtu;force_notify=1;
    }
    taskEXIT_CRITICAL();
    if(reject!=0xff) (void)gap_disconnect(reject);
}
void et100_app_server_poll(void)
{
    uint8_t value[ET100_MANAGER_SELECTED_MAX];
    taskENTER_CRITICAL();
    uint32_t now=now_ms();
    et100_manager_expire(&manager,now);
    if(app_link==0xff || service_id==0xff || !subscribed || in_flight ||
       (uint32_t)(now-last_attempt)<100) goto done;
    size_t n=et100_manager_selected(&manager,now,value,sizeof(value));
    /* Never send a truncated selected header/payload at the default MTU. */
    if(!n || mtu<3 || n>(size_t)(mtu-3)) goto done;
    if(!force_notify && n==notify_length && !memcmp(value,notify_value,n)) goto done;
    memcpy(notify_value,value,n);notify_length=(uint16_t)n;
    struct gatt_send_event notification={.conidx=app_link,.att_idx=DATA,
        .p_data=notify_value,.data_len=notify_length,.svc_id=service_id};
    unsigned sending_generation=generation;
    in_flight=1;force_notify=0;last_attempt=now;
    /* Publish pending before the SDK call; a completion may be immediate. */
    ble_api_err_t error=gatt_notification(&notification);
    if(error && generation==sending_generation) {
        in_flight=0;force_notify=1;et100_app_notify_errors++;
    }
done:
    taskEXIT_CRITICAL();
}
