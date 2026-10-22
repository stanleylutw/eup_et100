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
static uint8_t identity[6];
/* Response queue of 2 (as eup_gateway). A slot is freed as soon as its last
 * part is handed to the stack, so a client that sends its next command right
 * after the reply is never treated as busy. All queue and link state is read
 * and written only inside critical sections (BLE task vs serial-task poll). */
struct slot {
    struct eup_meta_response r;
    size_t chunk;
    uint8_t next_part, total, gen;
};
static struct slot queue[2];
static unsigned head, count;
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

static void drop_all(void)
{
    head = count = 0;
    outstanding = 0;
}

static uint16_t server_callback(struct gatt_msg *msg)
{
    uint16_t result = 0;
    taskENTER_CRITICAL();
    if (msg->conn_idx != link || link == 0xff) goto done;
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
            if (count >= 2 || !subscribed) {
                et100_meta_dropped++;          /* gateway: queue full / no CCCD -> no reply */
            } else {
                struct slot *s = &queue[(head + count) % 2];
                struct eup_meta_identity id = { .info = &et100_fw_info, .mtu = mtu };
                memcpy(id.mac, identity, sizeof id.mac);
                eup_meta_handle(p, n, &id, &s->r);
                s->chunk = eup_meta_chunk_max(mtu);
                s->total = eup_meta_part_total(&s->r, s->chunk);
                s->next_part = 0;
                s->gen = (uint8_t)generation;
                count++;
            }
        }
    }
done:
    taskEXIT_CRITICAL();
    return result;
}

uint8_t et100_meta_add_service(void)
{
    struct gatt_service service = {.p_att_tb = attributes, .att_nb = ATTRIBUTE_COUNT,
                                   .gatt_msg_handler = server_callback};
    et100_identity_mac(identity);              /* once, outside BLE callbacks */
    service_id = gatt_add_service(&service);
    return service_id;
}

void et100_meta_gap(struct gap_event *event)
{
    taskENTER_CRITICAL();
    if (event->type == GAP_EVT_SLAVE_CONNECT && link == 0xff) {
        link = event->param.connect.conidx;
        mtu = 23;
        subscribed = 0;
        drop_all();
        generation++;
    } else if (event->type == GAP_EVT_DISCONNECT && event->param.disconnect.conidx == link) {
        link = 0xff;
        subscribed = 0;
        drop_all();
        generation++;
    } else if (event->type == GATT_EVT_MTU && event->param.mtu_ind.conidx == link) {
        mtu = event->param.mtu_ind.mtu;
    }
    taskEXIT_CRITICAL();
}

void et100_meta_poll(void)
{
    static uint8_t packet[EUP_META_RESPONSE_OVERHEAD + EUP_META_MAX_RESPONSE_PAYLOAD];
    /* Check, encode, notify and advance as one step: a disconnect/reconnect in
     * the BLE task can never interleave and send an old part to a new peer.
     * Same pattern as et100_app_server_poll (gatt_notification in a critical section). */
    taskENTER_CRITICAL();
    while (count && !outstanding) {
        struct slot *s = &queue[head];
        if (link == 0xff || !subscribed || s->gen != (uint8_t)generation || s->next_part >= s->total) {
            head = (head + 1) % 2;             /* stale or finished: drop */
            count--;
            continue;
        }
        size_t n = eup_meta_encode_part(&s->r, s->chunk, s->next_part, packet, sizeof packet);
        if (!n) { head = (head + 1) % 2; count--; continue; }
        struct gatt_send_event notification = {.conidx = link, .att_idx = TX,
            .p_data = packet, .data_len = (uint16_t)n, .svc_id = service_id};
        if (gatt_notification(&notification)) break;   /* retry this part next poll */
        outstanding++;
        if (++s->next_part >= s->total) {      /* last part handed over: free the slot now */
            head = (head + 1) % 2;
            count--;
        }
        break;                                 /* one part per completion */
    }
    taskEXIT_CRITICAL();
}
