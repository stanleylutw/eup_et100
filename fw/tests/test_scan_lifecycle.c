#include <assert.h>
#include <stdio.h>
#include "../src/ble_reporter.c"

static uint32_t ticks;
static unsigned calls, mode, stops, stop_mode, app_reports;
static struct gap_scan_param last_params;
static char output[128];
void et100_app_server_clear(void) {}
void et100_app_server_receive(const uint8_t mac[6],uint8_t type,int8_t rssi,uint8_t adv_type,
    const uint8_t *raw,size_t n,const struct et100_adv_details *details,uint32_t now)
{ (void)mac;(void)type;(void)rssi;(void)adv_type;(void)raw;(void)n;(void)details;(void)now;app_reports++; }
uint32_t xTaskGetTickCount(void) { return ticks; }
static void started(unsigned status)
{
    struct gap_event event = {.type = GAP_EVT_SCAN_START};
    event.param.scan_start_status = (uint8_t)status;
    et100_scan_event(&event);
}
static void ended(void)
{
    struct gap_event event = {.type = GAP_EVT_SCAN_END};
    et100_scan_event(&event);
}
ble_api_err_t gap_scan_start(struct gap_scan_param *params)
{
    assert(params->scan_type == GAP_SCAN_TYPE_GEN_DISC && params->duration == 0);
    assert(params->scan_intv == 40 && !params->dup_filt_pol);
    assert(params->scan_window == 10 || params->scan_window == 30);
    assert(et100_scan_status == 0xfe && et100_scan_diag.phase == ET100_SCAN_START_PENDING);
    last_params = *params;calls++;
    if (mode == 2) return BLE_API_ERR_UNKNOWN;
    if (mode != 1) started(mode == 3 ? BLE_API_ERR_NO_AVAILABLE_RESOURCE : 0);
    return BLE_API_ERR_NO_ERR;
}
ble_api_err_t gap_scan_stop(void)
{
    assert(et100_scan_diag.phase == ET100_SCAN_STOP_PENDING);
    stops++;
    if (stop_mode == 2) return BLE_API_ERR_UNKNOWN;
    if (stop_mode == 0) ended();
    return BLE_API_ERR_NO_ERR;
}
static void reset_scan(void)
{
    ticks = calls = stops = mode = stop_mode = app_reports = 0;
    scan_attempt_ms = scan_operation_ms = scan_stop_attempt_ms = 0;
    scan_attempted = scan_stop_attempted = connected_scan = connected_scan_attempted = 0;
    et100_scan_status = 0xff;
    et100_scan_diag = (struct et100_scan_diagnostics){.desired_window=30};
}
static void capture(const char *line,size_t length,void *context)
{
    (void)context;assert(length < sizeof output);
    memcpy(output,line,length);output[length]=0;
}
static void reporter_during_connected_scan(void)
{
    reset_scan();
    const uint16_t cids[4] = {0x0639};
    uint8_t macs[8][6] = {{0}};
    et100_scan_configure(cids,1,macs,1,1);
    et100_scan_connected_mode(1);et100_scan_start();
    assert(calls==1 && et100_scan_status==0 && last_params.scan_window==10);
    uint8_t payload[]={3,0xff,0x39,0x06};
    struct gap_evt_adv_report report={.data=payload};
    struct gap_evt_adv_report *ad=&report;
    ad->evt_type=GAP_SCAN_EVT_NONCONN_NONSCAN;ad->rssi=-50;ad->length=4;
    memcpy(ad->addr.addr.addr,(uint8_t[]){0x66,0x55,0x44,0x33,0x22,0x11},6);
    struct gap_event event={.type=GAP_EVT_ADV_REPORT};event.param.adv_rpt=ad;
    et100_scan_event(&event);
    assert(app_reports==1);
    ticks=1000;output[0]=0;et100_scan_poll(capture,NULL);
    assert(strstr(output,"AT$PD=G1;1;112233445566;3906;0050*")==output);
    char *star=strchr(output,'*');unsigned expected=0;uint8_t checksum=0;
    assert(star && sscanf(star+1,"%2x",&expected)==1);
    for(char *p=output+6;p<star;p++)checksum^=(uint8_t)*p;
    assert(checksum==expected && !strcmp(star+3,"\r\n"));
    /* CID filtering and exclusion of scan responses remain unchanged. */
    payload[2]=0x38;et100_scan_event(&event);assert(app_reports==1);
    payload[2]=0x39;ad->evt_type=GAP_SCAN_EVT_SCAN_RSP;et100_scan_event(&event);assert(app_reports==1);
}
int main(void)
{
    reset_scan();
    const uint16_t cids[4] = {0x0639};
    uint8_t macs[8][6] = {{0xaa,0xbb,0xcc,0xdd,0xee,0xff}};
    et100_scan_configure(cids,15,macs,8,1);
    assert(reporter.maxrpt==8 && !memcmp(reporter.macs,macs,sizeof macs));
    et100_scan_configure(cids,10,macs,8,0);
    assert(reporter.gtime==10 && reporter.maxrpt==8);
    et100_scan_configure(cids,10,macs,4,1);
    assert(reporter.maxrpt==4);
    et100_scan_start();assert(calls==1 && et100_scan_status==0 && last_params.scan_window==30);
    et100_scan_start();assert(calls==1);
    ended();ticks=1000;mode=3;et100_scan_start();
    assert(calls==2 && et100_scan_status==0x204 && et100_scan_diag.start_failed==1);
    ticks=1500;et100_scan_start();assert(calls==2);
    ticks=2000;mode=0;et100_scan_start();assert(calls==3 && et100_scan_status==0);
    /* A low-duty profile replaces an existing normal scan only after END. */
    et100_scan_connected_mode(1);stop_mode=1;et100_scan_start();
    assert(stops==1 && et100_scan_diag.phase==ET100_SCAN_STOP_PENDING);
    ticks=4000;et100_scan_start();assert(stops==1 && calls==3);
    ended();et100_scan_start();assert(calls==4 && et100_scan_diag.phase==ET100_SCAN_ACTIVE);
    assert(et100_scan_diag.active_window==10);
    started(0);assert(et100_scan_diag.unexpected_events==1);
    ended();ticks=9000;et100_scan_start();assert(calls==4); /* once per connected session */
    /* A new session may try once again; synchronous and asynchronous failures are distinct. */
    et100_scan_connected_mode(0);et100_scan_connected_mode(1);mode=2;et100_scan_start();
    assert(calls==5 && et100_scan_status==0xfd);
    ticks+=5000;et100_scan_start();assert(calls==5);
    et100_scan_connected_mode(0);et100_scan_connected_mode(1);mode=3;et100_scan_start();
    assert(calls==6 && et100_scan_status==0x204 && et100_scan_diag.last_start_status==4);
    ticks+=5000;et100_scan_start();assert(calls==6);
    /* Missing START: do not enqueue a second start; stop and await END first. */
    reset_scan();mode=1;et100_scan_connected_mode(1);et100_scan_start();
    assert(calls==1 && !et100_scan_diag.start_success && et100_scan_status==0xfe);
    ticks=2999;et100_scan_start();assert(calls==1 && !stops);
    ticks=3000;stop_mode=1;et100_scan_start();
    assert(calls==1 && stops==1 && et100_scan_diag.start_timeouts==1);
    started(0); /* A late START cannot overwrite the pending STOP. */
    assert(et100_scan_diag.phase==ET100_SCAN_STOP_PENDING && et100_scan_status!=0);
    ticks=6000;et100_scan_start();
    assert(stops==1 && calls==1 && et100_scan_diag.stop_timeouts==1 && et100_scan_status==0xfa);
    ticks=9000;et100_scan_start();assert(stops==1 && et100_scan_diag.stop_timeouts==1);
    et100_scan_connected_mode(0);et100_scan_start();assert(calls==1);
    ended();mode=0;et100_scan_start();assert(calls==2 && last_params.scan_window==30);
    /* Disconnect while START is pending: finish the old start, stop, then restore normal. */
    reset_scan();mode=1;et100_scan_connected_mode(1);et100_scan_start();
    et100_scan_connected_mode(0);et100_scan_start();assert(calls==1 && stops==0);
    started(0);stop_mode=0;et100_scan_start();assert(stops==1);
    ticks=1000;mode=0;et100_scan_start();assert(calls==2 && last_params.scan_window==30);
    /* Stop rejection is throttled and cannot trigger a conflicting start. */
    reset_scan();et100_scan_connected_mode(1);et100_scan_start();
    et100_scan_connected_mode(0);stop_mode=2;et100_scan_start();
    assert(stops==1 && et100_scan_diag.phase==ET100_SCAN_UNCERTAIN);
    ticks=999;et100_scan_start();assert(stops==1 && calls==1);
    ticks=1000;stop_mode=0;et100_scan_start();assert(stops==2 && calls==1);
    et100_scan_start();assert(calls==2 && last_params.scan_window==30);
    /* All operation/backoff deadlines must survive tick wrap. */
    reset_scan();ticks=UINT32_MAX-100;mode=1;et100_scan_start();
    ticks=2898;et100_scan_start();assert(stops==0);
    ticks=2899;et100_scan_start();assert(stops==1 && et100_scan_diag.start_timeouts==1);
    ticks=2900;mode=0;et100_scan_start();assert(calls==2 && et100_scan_status==0);
    reporter_during_connected_scan();
    puts("PASS scan profiles: event-confirmed start, once-per-link low duty, serialized normal restore,");
    puts("     sync/async errors, late/missing callbacks, bounded stop, tick wrap and other-beacon Type-1");
    return 0;
}
