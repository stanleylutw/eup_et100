#include <assert.h>
#include <stdio.h>
#include "../src/ble_reporter.c"

static uint32_t ticks;
static unsigned calls, mode;
void et100_app_server_clear(void) {}
void et100_app_server_receive(const uint8_t mac[6],uint8_t type,int8_t rssi,uint8_t adv_type,
    const uint8_t *raw,size_t n,const struct et100_adv_details *details,uint32_t now)
{ (void)mac;(void)type;(void)rssi;(void)adv_type;(void)raw;(void)n;(void)details;(void)now; }
uint32_t xTaskGetTickCount(void) { return ticks; }
ble_api_err_t gap_scan_start(struct gap_scan_param *params)
{
    assert(params->scan_type == GAP_SCAN_TYPE_GEN_DISC);
    assert(et100_scan_status == 0xfe);
    calls++;
    if (mode == 2) return BLE_API_ERR_UNKNOWN;
    if (mode != 1) {
        struct gap_event event = {.type = GAP_EVT_SCAN_START};
        event.param.scan_start_status = mode == 3 ? BLE_API_ERR_NO_AVAILABLE_RESOURCE : 0;
        et100_scan_event(&event);
    }
    return BLE_API_ERR_NO_ERR;
}
static void ended(void)
{
    struct gap_event event = {.type = GAP_EVT_SCAN_END};
    et100_scan_event(&event);
}
int main(void)
{
    et100_scan_start(); assert(calls == 1 && et100_scan_status == 0);
    et100_scan_start(); assert(calls == 1);
    ended(); ticks = 1000; mode = 3;
    et100_scan_start(); assert(calls == 2 && et100_scan_status == 0x204);
    ticks = 1500; et100_scan_start(); assert(calls == 2);
    ticks = 2000; mode = 0; et100_scan_start(); assert(calls == 3 && et100_scan_status == 0);
    ended(); ticks = 3000; mode = 1; et100_scan_start(); assert(calls == 4 && et100_scan_status == 0xfe);
    ticks = 4000; et100_scan_start(); assert(calls == 4);
    ticks = 5999; et100_scan_start(); assert(calls == 4);
    ticks = 6000; mode = 2; et100_scan_start(); assert(calls == 5 && et100_scan_status == 0xfd);
    ticks = 6500; et100_scan_start(); assert(calls == 5);
    ticks = 7000; mode = 0; et100_scan_start(); assert(calls == 6 && et100_scan_status == 0);
    puts("PASS: scan callback publication, async/sync failure, retry backoff, pending timeout, restart");
    return 0;
}
