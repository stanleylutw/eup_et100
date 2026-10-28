/* Source: eup_canbus_32f407/tests/host/test_can_protocol.c, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    test_can_protocol.c
 * @brief   CAN parser and exact reply regression tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host-only independent golden bytes plus malformed, failure, token and range coverage.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#include "app_can_protocol.h"
#include "app_gateway.h"
#include "can_v19_fakes/vectors.h"
#include <stdlib.h>
/******************************************************************************
 * @brief   reply — CAN parser and exact reply regression tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host-only independent golden bytes plus malformed, failure, token and range coverage.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host-only independent golden bytes plus malformed, failure, token and range coverage.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static void reply(const char *line,unsigned index,int success)
{
    CanRequest_t r;char out[160];assert(CanProtocol_Parse(line,&r));
    assert(CanProtocol_Reply(&r,&CAN_CONFIG_DEFAULT,26,success,out,sizeof(out)));
    assert(!strcmp(out,golden[index]));
}
/******************************************************************************
 * @brief   main — CAN parser and exact reply regression tests.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Host-only independent golden bytes plus malformed, failure, token and range coverage.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Host-only independent golden bytes plus malformed, failure, token and range coverage.
 * @edge_case Bounded input and unsigned millisecond deltas prevent overflow and stale access.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int main(int argc,char **argv)
{
    assert(argc==2);vectors(argv[1]);
    reply("CAN$GGTIME;12AB",9,1);reply("CAN$SCARTYPE;2;12AB",10,0);
    reply("CAN$STYPESWITCH;1:1:1:0;12AB",11,0);reply("CAN$GVERSION;33AB",12,1);
    CanRequest_t r;CanConfig_t next;char out[160],request[160];
    assert(CanProtocol_Parse("CAN$STYPESWITCH;0:1:0:1;a0BZ",&r));assert(CanProtocol_Candidate(&r,&CAN_CONFIG_DEFAULT,&next));assert(next.types==10U);
    assert(CanProtocol_Parse("CAN$SGTIME;9999;12AB",&r));assert(CanProtocol_Candidate(&r,&CAN_CONFIG_DEFAULT,&next) && next.gtime==9999U);
    assert(CanProtocol_Parse("CAN$SGTIME;0;12AB",&r) && !r.valid);
    assert(CanProtocol_Parse("CAN$SGTIME;10000;12AB",&r) && !r.valid);
    assert(CanProtocol_Parse("CAN$SBITRATE;3;12AB",&r) && !r.valid);
    assert(CanProtocol_Parse("CAN$STYPESWITCH;2:1:1:1;12AB",&r) && !r.valid);
    assert(CanProtocol_Reply(&r,&CAN_CONFIG_DEFAULT,26,0,out,sizeof(out)));assert(strstr(out,";0;0:0:0:0;12AB*"));
    assert(CanProtocol_Parse("CAN$GVERSION;33AB",&r));assert(CanProtocol_Reply(&r,0,26,0,out,sizeof(out)));assert(strstr(out,";0;V1:26;33AB*"));
    assert(!CanProtocol_Parse("CAN$GGTIME;ABC",&r));assert(!CanProtocol_Parse("CAN$GGTIME;AB!D",&r));
    assert(!CanProtocol_Parse("CAN$GGTIME;ABCD*00",&r));assert(!CanProtocol_Parse("CAN$GGTIME;ABCD\n",&r));
    assert(CanProtocol_Parse("CAN$GNOTREAL;ABCD",&r));assert(r.command==CAN_CMD_UNKNOWN);
    size_t n=AppGateway_Wire("CAN$GGTIME;ABCD",out,sizeof(out));assert(n);strcpy(request,out+6);request[strlen(request)-2]=0;
    assert(CanProtocol_Parse(request,&r) && r.valid);
    srand(19);for(unsigned k=0;k<10000U;k++)
    { unsigned count=(unsigned)rand()%159;for(unsigned i=0;i<count;i++) { request[i]=(char)(1+(unsigned)rand()%126U); }request[count]=0;(void)CanProtocol_Parse(request,&r); }
    puts("PASS CAN protocol: four exact golden replies, failure echo, ranges, optional XOR, malformed token and 10000 sanitizer fuzz cases");return 0;
}
