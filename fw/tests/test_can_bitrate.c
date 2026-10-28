/* Source: eup_canbus_32f407/tests/host/test_can_bitrate.c, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    test_can_bitrate.c
 * @brief   Scan timing, errors, lock and wrap tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Fake hardware apply records mode/rate; production state machine runs without sleeps.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#include "app_can_bitrate.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls,kbps;static int silent,fail;
/******************************************************************************
 * @brief   apply — Scan timing, errors, lock and wrap tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Fake hardware apply records mode/rate; production state machine runs without sleeps.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Fake hardware apply records mode/rate; production state machine runs without sleeps.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static int apply(void *context,unsigned rate,int listen)
{ (void)context;calls++;kbps=rate;silent=listen;return !fail; }
/******************************************************************************
 * @brief   main — Scan timing, errors, lock and wrap tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Fake hardware apply records mode/rate; production state machine runs without sleeps.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Fake hardware apply records mode/rate; production state machine runs without sleeps.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int main(void)
{
    CanBitrate_t r;CanBitrate_Init(&r,apply,0);assert(!calls);
    assert(CanBitrate_Set(&r,0,0) && kbps==250 && silent);CanBitrate_Poll(&r,499);assert(calls==1);
    CanBitrate_Poll(&r,500);assert(kbps==500 && silent);
    CanBitrate_Observe(&r,501,1);CanBitrate_Observe(&r,502,1);CanBitrate_Observe(&r,503,0);assert(!r.locked);
    CanBitrate_Observe(&r,504,1);CanBitrate_Observe(&r,505,1);CanBitrate_Observe(&r,506,1);assert(r.locked && !silent && kbps==500);
    CanBitrate_Poll(&r,5505);assert(r.locked);CanBitrate_Poll(&r,5506);assert(!r.locked && silent && kbps==250);
    assert(CanBitrate_Set(&r,2,0));unsigned before=calls;CanBitrate_Poll(&r,100000);assert(calls==before && r.locked && kbps==500);
    assert(!CanBitrate_Set(&r,3,0) && calls==before);
    fail=1;assert(!CanBitrate_Set(&r,1,100));assert(!r.running && r.setting==2);
    CanBitrate_Poll(&r,599);assert(calls==before+1);fail=0;CanBitrate_Poll(&r,600);assert(r.running && r.setting==2);
    assert(CanBitrate_Set(&r,0,UINT32_MAX-100));CanBitrate_Poll(&r,398);assert(kbps==250);CanBitrate_Poll(&r,399);assert(kbps==500);
    for(unsigned i=0;i<10000;i++) { CanBitrate_Poll(&r,399+i*500U);assert(!r.locked); }
    puts("PASS CAN scan: exact 500ms windows, three-frame lock, errors, 5s silence, fixed rates, HAL failure recovery, wrap, idle bus");return 0;
}
