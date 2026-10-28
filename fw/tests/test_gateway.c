/* Source: eup_canbus_32f407/tests/host/test_gateway.c, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    test_gateway.c
 * @brief   V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#include "app_gateway.h"
#include "can_v19_fakes/vectors.h"
#include <stdint.h>
static char output[8192];static size_t used;
/******************************************************************************
 * @brief   capture — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void capture(const char *p,size_t n)
{ assert(n<160 && used+n<sizeof(output));memcpy(output+used,p,n);used+=n;output[used]=0; }
/******************************************************************************
 * @brief   clear — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void clear(void) { used=0;output[0]=0; }
/******************************************************************************
 * @brief   init — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void init(uint8_t types)
{ AppGateway_Init(capture,0,1);CanConfig_t c={1,30,types,0};AppGateway_Configure(&c,0);clear(); }
/******************************************************************************
 * @brief   frame — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void frame(uint32_t pgn,const uint8_t *bytes,uint32_t now)
{ AppGateway_Frame((6U<<26)|(pgn<<8),bytes,8,now); }
/******************************************************************************
 * @brief   value — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void value(uint32_t pgn,unsigned byte,uint32_t raw,unsigned width,uint32_t now)
{
    uint8_t data[8]={255,255,255,255,255,255,255,255};
    for(unsigned i=0;i<width;i++) { data[byte+i]=(uint8_t)(raw>>(8*i)); }frame(pgn,data,now);
}
/******************************************************************************
 * @brief   type2_fields — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void type2_fields(unsigned mileage,unsigned fuel,unsigned urea)
{
    value(65217,0,mileage,4,30000);value(65276,1,fuel,1,30000);value(65266,4,5120,2,30000);
    value(65129,2,(273+85)*32,2,30000);value(65262,2,(273-40)*32,2,30000);
    uint8_t data[8]={0};data[0]=(uint8_t)urea;data[1]=80;frame(65110,data,30000);
    value(65257,4,100,4,30000);
}
/******************************************************************************
 * @brief   test_golden — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void test_golden(void)
{
    char line[160];CanConfig_t config={1,30,15,0};
    assert(AppGateway_BootLine(&config,26,line,sizeof(line)));assert(!strcmp(line,golden[0]));
    assert(!AppGateway_Wire("123",line,4));assert(!line[0]);
    init(1);value(61444,6,3,1,0);value(61445,3,126,1,0);value(65265,0,4,1,0);AppGateway_Poll(0);
    assert(!strcmp(output,golden[1]));init(1);AppGateway_Poll(0);assert(!strcmp(output,golden[2]));
    init(2);type2_fields(80040,120,125);AppGateway_Poll(30000);assert(!strcmp(output,golden[3]));
    init(2);type2_fields(199,2,2);AppGateway_Poll(30000);assert(!strcmp(output,golden[4]));
    init(2);AppGateway_Poll(30000);assert(!strcmp(output,golden[5]));
    init(4);AppGateway_Invalidate(10000);value(65132,6,60*256,2,10000);value(61444,3,900*8,2,10000);AppGateway_Poll(10000);
    assert(!strcmp(output,golden[6]));
    init(8);AppGateway_Invalidate(10000);value(61449,0,62127,2,10000);AppGateway_Poll(10000);assert(!strcmp(output,golden[7]));
    init(8);AppGateway_Invalidate(10000);AppGateway_Poll(10000);assert(!strcmp(output,golden[8]));
}
/******************************************************************************
 * @brief   test_validity — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void test_validity(void)
{
    init(1);value(61444,6,14,1,0);value(61445,3,255,1,0);value(65265,0,4,1,0);AppGateway_Poll(0);
    assert(strstr(output,";14;255;1*"));clear();AppGateway_Poll(2999);assert(!used);
    AppGateway_Poll(3000);assert(!strcmp(output,golden[2]));
    for(unsigned gear=0;gear<=255;gear++)
    {
        init(1);value(61445,3,gear,1,0);AppGateway_Poll(0);
        char expected[32];snprintf(expected,sizeof(expected),";000000;;%u;*",gear);assert(strstr(output,expected));
    }
    init(1);value(61444,6,3,1,0);AppGateway_Poll(0);clear();value(61444,6,15,1,1);AppGateway_Poll(1);
    assert(!strcmp(output,golden[2]));
    init(1);value(61445,3,126,1,0);AppGateway_Poll(0);clear();uint8_t short_data[1]={0};
    AppGateway_Frame(0x18F00500U,short_data,1,1);AppGateway_Poll(1);assert(!strcmp(output,golden[2]));
    init(2);value(65217,0,200,4,0);AppGateway_Poll(30000);assert(strstr(output,";000000;1;"));
    clear();AppGateway_Poll(89999);assert(strstr(output,";000000;1;"));clear();AppGateway_Poll(119999);assert(strstr(output,";000000;;;;;;:;"));
    init(1);value(61444,6,3,1,UINT32_MAX-1000);AppGateway_Poll(1998);assert(strstr(output,";3;;"));clear();AppGateway_Poll(1999);assert(!strcmp(output,golden[2]));
}
/******************************************************************************
 * @brief   test_numeric — V1.9 golden wire, boundary and lifetime tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void test_numeric(void)
{
    const unsigned raw[]={0,32126,32127,32128,64254,64255};
    const char *packed[]={"07D7F","00001","10000","10001","17D7F","17D80"};
    for(unsigned i=0;i<6;i++)
    { init(8);AppGateway_Invalidate(10000);value(61449,0,raw[i],2,10000);AppGateway_Poll(10000);assert(strstr(output,packed[i])); }
    init(2);value(65129,2,7440,2,30000);value(65262,2,8720,2,30000);value(65257,4,16777217,4,30000);AppGateway_Poll(30000);
    assert(strstr(output,";040;10;:;83886085*"));
    init(2);value(65217,0,4211081215U,4,30000);value(65257,4,4211081215U,4,30000);AppGateway_Poll(30000);
    assert(strstr(output,";21055406;"));assert(strstr(output,";21055406075*"));
    init(4);AppGateway_Invalidate(10000);value(65132,6,65535,2,10000);value(61443,1,255,1,10000);value(61441,1,255,1,10000);AppGateway_Poll(10000);
    assert(strstr(output,"00FFFF6666*"));
    init(4);AppGateway_Poll(0);value(65132,6,60*256,2,30000);AppGateway_Poll(30000);
    assert(strstr(output,";10;3CFFFFFFFFFFFFFFFFFF")); /* Only newest slot is real; nine missed seconds are F. */
    init(15);assert(AppGateway_NextMode()==APP_GATEWAY_J1939);clear();value(61444,3,7200,2,0);assert(strstr(output,"18F00400,8,"));
    assert(AppGateway_NextMode()==APP_GATEWAY_ASCII);clear();value(61444,3,7200,2,0);assert(strstr(output,"SPN190=900rpm"));
    assert(AppGateway_NextMode()==APP_GATEWAY_RS232);
}
/******************************************************************************
 * @brief   Verify D16 positions against legacy-byte decoys.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host ASan/UBSan tests exercise production encoding with independently reviewed vectors, no device access.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void test_d16(void)
{
    /* Wrong legacy bytes are deliberate decoys; only D16 positions may affect output. */
    uint8_t rpm[8]={0xF0,255,255,0x20,0x1C,255,3,255};
    uint8_t gear[8]={255,255,255,126,125,255,255,255};
    init(1);frame(61444,rpm,0);frame(61445,gear,0);AppGateway_Poll(0);assert(strstr(output,";3;126;*"));
    clear();AppGateway_Frame(0x18F00400U,rpm,6,1);AppGateway_Poll(1);assert(strstr(output,";000000;;126;*"));
    init(4);AppGateway_Invalidate(10000);
    uint8_t speed[8]={255,0,65,255,255,255,0,60};
    uint8_t brake[8]={100,50,255,255,255,255,255,255};
    frame(65132,speed,10000);frame(61441,brake,10000);AppGateway_Poll(10000);assert(strstr(output,"3CFFFFFF14*"));
    init(4);AppGateway_Invalidate(10000);AppGateway_Frame(0x18FE6C00U,speed,7,10000);AppGateway_Poll(10000);assert(strstr(output,"FFFFFFFFFF*"));
}
/******************************************************************************
 * @brief   Run gateway conformance tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host process loads reviewed golden wires, then asserts encoding, TTL, precision, sampling gaps, S3 and D16 positions under ASan/UBSan; failures abort the test process.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host process loads reviewed golden wires, then asserts encoding, TTL, precision, sampling gaps, S3 and D16 positions under ASan/UBSan; failures abort the test process.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int main(int argc,char **argv)
{ assert(argc==2);vectors(argv[1]);test_golden();test_validity();test_numeric();test_d16();puts("PASS V1.9 gateway: 9 golden wires, D11/D14/D15/D16, TTL, N/A, DLC, wrap, 64-bit precision, sampling gaps, S3");return 0; }
