/* Source: eup_canbus_32f407/tests/host/can_v19_fakes/vectors.h, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    vectors.h
 * @brief   Read independent plan golden wires.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host-only bounded ASCII JSON wire reader; expected bytes come from the reviewed fixture, not the encoder.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#ifndef CAN_TEST_VECTORS_H
#define CAN_TEST_VECTORS_H
#include <stdio.h>
#include <string.h>
#include <assert.h>
static char golden[13][160];
/******************************************************************************
 * @brief   vectors — Read independent plan golden wires.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host-only bounded ASCII JSON wire reader; expected bytes come from the reviewed fixture, not the encoder.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host-only bounded ASCII JSON wire reader; expected bytes come from the reviewed fixture, not the encoder.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void vectors(const char *path)
{
    FILE *f=fopen(path,"r");assert(f);char buffer[1024];unsigned n=0;
    while(fgets(buffer,sizeof(buffer),f))
    {
        char *p=strstr(buffer,"\"wire\": \"");if(!p) { continue; }p+=9;unsigned at=0;
        assert(n<13);
        while(*p && *p!='"')
        {
            char c=*p++;
            if(c=='\\') { c=*p++;c=c=='r' ? '\r':c=='n' ? '\n':c; }
            assert(at+1<sizeof(golden[n]));golden[n][at++]=c;
        }
        golden[n++][at]=0;
    }
    fclose(f);assert(n==13);
}
#endif
