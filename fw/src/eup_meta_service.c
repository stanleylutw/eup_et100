#include "eup_meta_service.h"
#include "eup_meta_protocol.h"
#include "fr30xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gap_api.h"
#include "gatt_api.h"
#include <string.h>

extern const struct eup_fw_info et100_fw_info;

enum { SERVICE, RX_DECL, RX, TX_DECL, TX, CCC, ATTRIBUTE_COUNT };
static const uint8_t service_uuid[] = EUP_META_UUID(0x00);
static const gatt_attribute_t attributes[] = {
    [SERVICE] = {{UUID_SIZE_2, UUID16_ARR(GATT_PRIMARY_SERVICE_UUID)}, GATT_PROP_READ, 16, (uint8_t *)service_uuid},
    [RX_DECL] = {{UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)}, GATT_PROP_READ, 0, NULL},
    [RX]      = {{16, EUP_META_UUID(0x01)}, GATT_PROP_WRITE_REQ | GATT_PROP_WRITE_CMD, 6 + EUP_META_MAX_REQUEST_PAYLOAD + 1, NULL},
    [TX_DECL] = {{UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)}, GATT_PROP_READ, 0, NULL},
    [TX]      = {{16, EUP_META_UUID(0x02)}, GATT_PROP_NOTI, EUP_META_RESPONSE_OVERHEAD + EUP_META_MAX_RESPONSE_PAYLOAD, NULL},
    [CCC]     = {{UUID_SIZE_2, UUID16_ARR(GATT_CLIENT_CHAR_CFG_UUID)}, GATT_PROP_READ | GATT_PROP_WRITE_REQ, 2, NULL},
};
static uint8_t service_id = 0xff, link = 0xff;
static volatile uint16_t mtu = 23;
static volatile unsigned subscribed, outstanding, generation;
/* One response at a time: built in the BLE callback, sent part by part by poll. */
static struct eup_meta_response response;
static volatile bool pending;
static uint8_t next_part, part_total, response_generation;
static size_t chunk;
volatile unsigned et100_meta_dropped;

void et100_identity_mac(uint8_t mac[6])
{
    /* Same derivation as serial_task / app_btdm_init: unique-ID static address. */
    uint8_t address[] = {0x32, 0x32, 0x32, 0x32, 0x32, 0xc0};
    uint8_t id_length = 5;
    system_get_unique_ID(address, &id_length);
    address[5] = 0xc0;
    for (unsigned i = 0; i < 6; i++) mac[i] = address[5 - i];
}

static uint16_t server_callback(struct gatt_msg *msg)
{
    uint16_t result = 0;
    if (msg->conn_idx != link || link == 0xff) return 0;
    if (msg->msg_evt == GATTC_MSG_CMP_EVT) {
        if (msg->param.gatt_op_cmp.operation == GATT_OP_NOTIFY && outstanding) outstanding--;
    } else if (msg->msg_evt == GATTS_MSG_READ_REQ) {
        if (msg->att_idx == CCC && msg->param.gatt_data.p_msg_data &&
            msg->param.gatt_data.msg_len >= 2 && !msg->param.gatt_data.offset) {
            msg->param.gatt_data.p_msg_data[0] = (uint8_t)subscribed;
            msg->param.gatt_data.p_msg_data[1] = 0;
            result = 2;
        }
    } else if (msg->msg_evt == GATTS_MSG_WRITE_REQ) {
        const uint8_t *p = msg->param.gatt_data.p_msg_data;
        size_t n = msg->param.gatt_data.msg_len;
        if (msg->att_idx == CCC && p && n == 2 && p[1] == 0 && p[0] <= 1) {
            subscribed = p[0];
        } else if (msg->att_idx == RX && p && !msg->param.gatt_data.offset) {
            if (pending || !subscribed) {
                et100_meta_dropped++;          /* gateway also drops without notify */
            } else {
                struct eup_meta_identity id = { .info = &et100_fw_info, .mtu = mtu };
                et100_identity_mac(id.mac);
                eup_meta_handle(p, n, &id, &response);
                chunk = eup_meta_chunk_max(mtu);
                part_total = eup_meta_part_total(&response, chunk);
                next_part = 0;
                response_generation = (uint8_t)generation;
                pending = true;
            }
        }
    }
    return result;
}

uint8_t et100_meta_add_service(void)
{
    struct gatt_service service = {.p_att_tb = attributes, .att_nb = ATTRIBUTE_COUNT,
                                   .gatt_msg_handler = server_callback};
    service_id = gatt_add_service(&service);
    return service_id;
}

void et100_meta_gap(struct gap_event *event)
{
    if (event->type == GAP_EVT_SLAVE_CONNECT && link == 0xff) {
        link = event->param.connect.conidx;
        mtu = 23;
        subscribed = outstanding = 0;
        pending = false;
        generation++;
    } else if (event->type == GAP_EVT_DISCONNECT && event->param.disconnect.conidx == link) {
        link = 0xff;
        subscribed = outstanding = 0;
        pending = false;
        generation++;
    } else if (event->type == GATT_EVT_MTU && event->param.mtu_ind.conidx == link) {
        mtu = event->param.mtu_ind.mtu;
    }
}

void et100_meta_poll(void)
{
    static uint8_t packet[EUP_META_RESPONSE_OVERHEAD + EUP_META_MAX_RESPONSE_PAYLOAD];
    if (!pending || outstanding) return;
    if (link == 0xff || !subscribed || response_generation != (uint8_t)generation) {
        pending = false;                       /* connection changed: drop stale reply */
        return;
    }
    if (next_part >= part_total) {
        pending = false;
        return;
    }
    size_t n = eup_meta_encode_part(&response, chunk, next_part, packet, sizeof packet);
    if (!n) { pending = false; return; }
    struct gatt_send_event notification = {.conidx = link, .att_idx = TX,
        .p_data = packet, .data_len = (uint16_t)n, .svc_id = service_id};
    taskENTER_CRITICAL();
    outstanding++;
    taskEXIT_CRITICAL();
    if (gatt_notification(&notification)) {
        taskENTER_CRITICAL();
        if (outstanding) outstanding--;
        taskEXIT_CRITICAL();
        return;                                /* retry this part on the next poll */
    }
    next_part++;
}
