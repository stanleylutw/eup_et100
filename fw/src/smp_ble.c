#include "smp_ble.h"
#include <string.h>

#define REASSEMBLY_TIMEOUT_MS 2000u

void smp_reassembly_reset(struct smp_reassembly *r)
{
    r->length = 0;
    r->expected = 0;
}

int smp_reassembly_add(struct smp_reassembly *r, const uint8_t *p, size_t n, uint32_t now_ms)
{
    if (r->length && (uint32_t)(now_ms - r->last_ms) > REASSEMBLY_TIMEOUT_MS)
        smp_reassembly_reset(r);
    r->last_ms = now_ms;
    if (!p || !n) return -1;
    if (!r->length) {
        /* A new packet must start with a full 8-byte SMP header. */
        if (n < 8) return -1;
        size_t total = 8 + ((size_t)p[2] << 8 | p[3]);
        if (total > sizeof r->data) return -1;
        r->expected = total;
    }
    if (n > r->expected - r->length) {
        smp_reassembly_reset(r);
        return -1;
    }
    memcpy(r->data + r->length, p, n);
    r->length += n;
    return r->length == r->expected ? 1 : 0;
}

#ifndef ET100_SMP_BLE_HOST_TEST
#include "FreeRTOS.h"
#include "task.h"
#include "gap_api.h"
#include "gatt_api.h"
#include "ota_worker.h"

enum { SERVICE, CHAR_DECL, VALUE, CCC, ATTRIBUTE_COUNT };
static const uint8_t service_uuid[] = SMP_BLE_SERVICE_UUID;
static const gatt_attribute_t attributes[] = {
    [SERVICE]   = {{UUID_SIZE_2, UUID16_ARR(GATT_PRIMARY_SERVICE_UUID)}, GATT_PROP_READ, 16, (uint8_t *)service_uuid},
    [CHAR_DECL] = {{UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)}, GATT_PROP_READ, 0, NULL},
    [VALUE]     = {{16, SMP_BLE_CHAR_UUID}, GATT_PROP_WRITE_CMD | GATT_PROP_WRITE_REQ | GATT_PROP_NOTI, 512, NULL},
    [CCC]       = {{UUID_SIZE_2, UUID16_ARR(GATT_CLIENT_CHAR_CFG_UUID)}, GATT_PROP_READ | GATT_PROP_WRITE_REQ, 2, NULL},
};
static uint8_t service_id = 0xff, link = 0xff;
static volatile uint16_t mtu = 23;
/* outstanding: notifications sent minus completions seen. A completion that
 * arrives after a timeout is absorbed here instead of finishing a later send. */
static volatile unsigned subscribed, outstanding, notify_failed, generation;
static struct smp_reassembly rx;
volatile unsigned et100_smp_ble_drops;

static uint32_t now_ms(void) { return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS; }

static uint16_t server_callback(struct gatt_msg *msg)
{
    uint16_t result = 0;
    if (msg->conn_idx != link || link == 0xff) return 0;
    if (msg->msg_evt == GATTC_MSG_CMP_EVT) {
        if (msg->param.gatt_op_cmp.operation == GATT_OP_NOTIFY && outstanding) {
            if (msg->param.gatt_op_cmp.status) notify_failed = 1;
            outstanding--;
        }
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
        if (msg->param.gatt_data.offset || !p) {
            et100_smp_ble_drops++;
        } else if (msg->att_idx == CCC && n == 2 && p[1] == 0 && p[0] <= 1) {
            subscribed = p[0];
        } else if (msg->att_idx == VALUE) {
            int done = smp_reassembly_add(&rx, p, n, now_ms());
            if (done < 0) et100_smp_ble_drops++;
            if (done == 1) {
                if (!ota_worker_submit(OTA_TRANSPORT_BLE, rx.data, rx.length, generation)) et100_smp_ble_drops++;
                smp_reassembly_reset(&rx);
            }
        }
    }
    return result;
}

uint8_t smp_ble_add_service(void)
{
    struct gatt_service service = {.p_att_tb = attributes, .att_nb = ATTRIBUTE_COUNT,
                                   .gatt_msg_handler = server_callback};
    service_id = gatt_add_service(&service);
    return service_id;
}

void smp_ble_gap(struct gap_event *event)
{
    if (event->type == GAP_EVT_SLAVE_CONNECT && link == 0xff) {
        link = event->param.connect.conidx;
        mtu = 23;
        subscribed = outstanding = 0;
        smp_reassembly_reset(&rx);
        generation++;
    } else if (event->type == GAP_EVT_DISCONNECT && event->param.disconnect.conidx == link) {
        link = 0xff;
        subscribed = outstanding = 0;
        smp_reassembly_reset(&rx);
        generation++;
    } else if (event->type == GATT_EVT_MTU && event->param.mtu_ind.conidx == link) {
        mtu = event->param.mtu_ind.mtu;
    }
}

int smp_ble_send(const uint8_t *packet, size_t length, uint32_t tag)
{
    static uint8_t segment[244];
    size_t sent = 0;
    /* Request must belong to this connection; a new peer never gets an old reply. */
    if (generation != tag) return -1;
    while (sent < length) {
        if (link == 0xff || !subscribed || generation != tag) return -1;
        /* Drain: wait for every earlier notification (incl. a timed-out one). */
        for (unsigned waited = 0; outstanding; waited++) {
            if (waited >= 1000 || generation != tag) return -1;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        /* Check, copy, notify and count as one step so a disconnect/reconnect in
         * the BLE task cannot slip in and receive an old response segment. */
        size_t n = 0;
        int rc = 0;
        taskENTER_CRITICAL();
        if (link == 0xff || !subscribed || generation != tag) {
            rc = -1;
        } else {
            size_t room = mtu > 3 ? (size_t)mtu - 3 : 20;
            if (room > sizeof segment) room = sizeof segment;
            n = length - sent < room ? length - sent : room;
            memcpy(segment, packet + sent, n);
            struct gatt_send_event notification = {.conidx = link, .att_idx = VALUE,
                .p_data = segment, .data_len = (uint16_t)n, .svc_id = service_id};
            notify_failed = 0;
            if (gatt_notification(&notification)) rc = -1;
            else outstanding++;
        }
        taskEXIT_CRITICAL();
        if (rc) return -1;
        for (unsigned waited = 0; outstanding; waited++) {
            if (waited >= 1000 || generation != tag) return -1;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        if (notify_failed) return -1;
        sent += n;
    }
    return 0;
}

void smp_ble_respond(const uint8_t *packet, size_t length, uint32_t tag)
{
    if (smp_ble_send(packet, length, tag)) et100_smp_ble_drops++;
}
#endif
