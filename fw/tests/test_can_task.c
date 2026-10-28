/* ET-100 glue tests: unchanged F407 modules + fake board/storage. No EVB. */
#define ET100_CAN_HOST_TEST
#include "../src/can/app_gateway.h"
static void counted_invalidate(uint32_t now);
#define AppGateway_Invalidate counted_invalidate
#include "../src/can_task.c"
#undef AppGateway_Invalidate
static unsigned invalidations;
static void counted_invalidate(uint32_t now){invalidations++;AppGateway_Invalidate(now);}
#include <assert.h>
#include <stdio.h>
static uint8_t durable[16];
static bool exists,fail_write,corrupt_read,fail_apply,hardware_off;
static unsigned applies,writes,rate;
static uint8_t states[SYS_COUNT];
void et100_sys_set(unsigned index,unsigned value){assert(index==12);states[index-1]=(uint8_t)value;}
int et100_can_hw_apply(unsigned kbps){applies++;rate=kbps;return !fail_apply;}
int et100_can_hw_bus_off(void){return hardware_off;}
int et100_can_record_read(uint8_t record[16]){
    if(!exists)return 0;
    memcpy(record,durable,16);if(corrupt_read)record[15]^=1;return 1;
}
int et100_can_record_write(const uint8_t record[16]){
    writes++;if(fail_write)return 0;memcpy(durable,record,16);exists=true;return 1;
}
static char collected[16000];
static unsigned reports[5];
static void drain(bool stop){char line[160];size_t n;collected[0]=0;
    while((n=et100_can_take_line(line,sizeof line,stop))){assert(strlen(collected)+n<sizeof collected);strcat(collected,line);
        if(!strncmp(line,"AT$PD=67;",9)&&line[9]>='0'&&line[9]<='4')reports[line[9]-'0']++;}}
static void request(const char *line,uint32_t now){char fail[160];assert(!et100_can_command(line,fail,sizeof fail));et100_can_step(now);drain(false);}
static void reset(void){exists=fail_write=corrupt_read=fail_apply=hardware_off=false;writes=applies=invalidations=0;memset(reports,0,sizeof reports);et100_can_start(1);}
int main(void)
{
    unsigned major,minor,patch;
    assert(sscanf(EUP_FIRMWARE_VERSION,"%u.%u.%u",&major,&minor,&patch)==3);
    assert(minor<10&&patch<10&&ET100_CAN_VERSION==major*100+minor*10+patch);
    char version_reply[80],boot_prefix[80];
    snprintf(version_reply,sizeof version_reply,"CAN$GVERSION;1;V1:%u;ABCD*",ET100_CAN_VERSION);
    snprintf(boot_prefix,sizeof boot_prefix,"AT$PD=67;0;V1;%u;",ET100_CAN_VERSION);
    reset();assert(rate==250&&applies==1&&states[11]==2);
    request("CAN$GVERSION;ABCD",0);assert(strstr(collected,version_reply));
    request("CAN$SGTIME;42;ABCD",1);assert(strstr(collected,"CAN$SGTIME;1;42;ABCD*"));assert(writes==1);
    request("CAN$SGTIME;42;ABCD",2);assert(writes==1);
    et100_can_start(1);request("CAN$GGTIME;ABCD",3);assert(strstr(collected,";1;42;ABCD*"));
    fail_write=true;request("CAN$SGTIME;43;ABCD",4);assert(strstr(collected,"CAN$SGTIME;0;0;ABCD*"));assert(configuration.gtime==42);
    fail_write=false;corrupt_read=true;request("CAN$SGTIME;44;ABCD",5);assert(strstr(collected,"CAN$SGTIME;0;0;ABCD*"));assert(configuration.gtime==42);
    et100_can_start(1);assert(configuration.gtime==30);corrupt_read=false;
    reset();request("CAN$SBITRATE;2;ABCD",1);assert(configuration.bitrate==2&&rate==500);
    fail_write=true;request("CAN$SBITRATE;1;ABCD",2);assert(configuration.bitrate==2&&rate==500);
    fail_write=false;fail_apply=true;request("CAN$SBITRATE;1;ABCD",3);assert(configuration.bitrate==2);
    fail_apply=false;et100_can_step(503);assert(bitrate.running&&rate==500);
    reset();uint8_t payload[8]={0};
    et100_can_receive_isr(0x18f00400,payload,8,false,false,false);
    et100_can_receive_isr(0x18f00400,payload,8,true,true,false);
    et100_can_receive_isr(0x18f00400,payload,8,true,false,true);
    et100_can_receive_isr(0x18f00400,payload,9,true,false,false);
    assert(diagnostics.rejected==4);
    for(unsigned i=0;i<65;i++)et100_can_receive_isr(0x18f00400,payload,8,true,false,false);
    assert(diagnostics.received==64&&diagnostics.overflow==1);
    et100_can_step(50);assert(!bitrate.locked&&states[11]==3);
    for(unsigned i=0;i<2;i++)et100_can_receive_isr(0x18f00400,payload,8,true,false,false);
    et100_can_step(75);assert(!bitrate.locked);
    et100_can_receive_isr(0x18f00400,payload,8,true,false,false);
    et100_can_step(100);assert(bitrate.locked);
    et100_can_step(5100);assert(!bitrate.locked&&rate==250);
    et100_can_step(5600);assert(rate==500);
    et100_can_error_isr(false,true,100,130);et100_can_step(5600);assert(diagnostics.passive&&diagnostics.rec==130);
    hardware_off=true;et100_can_error_isr(true,true,255,128);et100_can_step(5650);assert(states[11]==4&&diagnostics.bus_off==1);
    et100_can_step(5700);assert(states[11]==5);
    et100_can_step(5750);assert(states[11]==5&&bitrate.running);
    hardware_off=false;et100_can_step(5800);assert(states[11]==2);
    reset();et100_can_set_paused(true);
    for(uint32_t now=0;now<300000;now+=50)et100_can_step(now);
    drain(true);assert(!strstr(collected,"AT$PD=67;"));
    et100_can_set_paused(false);et100_can_step(300000);drain(false);
    assert(strstr(collected,boot_prefix));
    et100_can_step(300050);drain(false);assert(!strstr(collected,"AT$PD=67;0;"));
    et100_can_set_paused(true);et100_can_step(330000);drain(false);assert(!collected[0]);
    request("CAN$GVERSION;ABCD",330001);assert(strstr(collected,version_reply));
    /* Resume Type1 on real data, not on a quiet scan's redundant snapshot. */
    et100_can_set_paused(false);
    et100_can_receive_isr(0x18f00400,payload,8,true,false,false);
    et100_can_step(335000);drain(false);assert(strstr(collected,"AT$PD=67;1;"));
    /* Queued unsolicited lines must be discarded if SMP starts before TX. */
    et100_can_step(340000);drain(true);assert(!collected[0]);
    et100_can_start(0);request("CAN$SGTIME;40;ABCD",0);assert(strstr(collected,"CAN$SGTIME;0;0;"));
    /* An explicit error invalidates all confidence from older queued frames. */
    reset();
    for(unsigned i=0;i<3;i++)et100_can_receive_isr(0x18f00400,payload,8,true,false,false);
    et100_can_error_isr(false,false,0,1);et100_can_step(50);assert(!bitrate.locked);
    for(unsigned i=0;i<3;i++) {
        et100_can_receive_isr(0x18f00400,payload,8,true,false,false);
        et100_can_step(100+i);assert(bitrate.locked==(i==2));
    }
    /* A deferred Type0 must not block replies while SMP suppresses reports. */
    reset();et100_can_step(300000);et100_can_set_paused(true);
    char failure[160];assert(!et100_can_command("CAN$GVERSION;ABCD",failure,sizeof failure));
    et100_can_step(300001);drain(true);
    assert(strstr(collected,version_reply)&&!strstr(collected,"AT$PD=67;"));
    drain(false);assert(strstr(collected,boot_prefix));
    drain(false);assert(!collected[0]);
    /* The immediate queue-full failure uses the same version as normal replies. */
    reset();
    for(unsigned i=0;i<7;i++)assert(!et100_can_command("CAN$GVERSION;ABCD",failure,sizeof failure));
    assert(et100_can_command("CAN$GVERSION;ABCD",failure,sizeof failure));
    char version_failure[80];
    snprintf(version_failure,sizeof version_failure,"CAN$GVERSION;0;V1:%u;ABCD*",ET100_CAN_VERSION);
    assert(strstr(failure,version_failure));
    /* Auto scanning a quiet bus must not request snapshots on every switch. */
    reset();
    for(uint32_t now=0;now<=30000;now+=50){et100_can_step(now);drain(false);}
    assert(applies==61&&invalidations==0&&!rx_since_invalidate);
    assert(reports[1]==1&&reports[2]==1&&reports[3]==3&&reports[4]==3);
    /* One data epoch is cleared once at rescan, even after Type1 TTL expires. */
    reset();et100_can_step(0);drain(false);
    const uint8_t gear[8]={255,255,255,251,255,255,255,255};
    for(unsigned i=0;i<3;i++)et100_can_receive_isr(0x18f00500,gear,8,true,false,false);
    et100_can_step(50);drain(false);assert(bitrate.locked&&rx_since_invalidate);
    assert(strstr(collected,"AT$PD=67;1;000000;;251;"));
    /* Rescan first selects the current 250k rate without reinitializing it;
     * the following 500k transition is the first actual invalidate. */
    for(uint32_t now=100;now<=5550;now+=50){et100_can_step(now);drain(false);}
    assert(invalidations==1&&!rx_since_invalidate&&!bitrate.locked);
    assert(strstr(collected,"AT$PD=67;1;000000;;;"));
    unsigned after_clear=reports[1];
    for(uint32_t now=5600;now<=30000;now+=50){et100_can_step(now);drain(false);}
    assert(invalidations==1&&reports[1]==after_clear);
    /* New data after a cleared epoch must still be invalidated on failure. */
    et100_can_receive_isr(0x18f00500,gear,8,true,false,false);
    et100_can_step(30001);drain(false);assert(rx_since_invalidate);
    unsigned failed_rate=diagnostics.kbps==250?500:250;
    fail_apply=true;now_ms=30002;assert(!apply(NULL,failed_rate,1));
    assert(invalidations==2&&!rx_since_invalidate&&diagnostics.status==6);
    assert(!apply(NULL,failed_rate,1)&&invalidations==2);
    /* BUS_OFF clears data once; repeated errors/recovery with no RX stay quiet. */
    reset();et100_can_step(0);drain(false);
    et100_can_receive_isr(0x18f00500,gear,8,true,false,false);
    et100_can_step(50);drain(false);assert(rx_since_invalidate);
    hardware_off=true;et100_can_error_isr(true,true,255,128);et100_can_step(100);drain(false);
    assert(invalidations==1&&diagnostics.status==4&&!rx_since_invalidate);
    assert(strstr(collected,"AT$PD=67;1;000000;;;"));
    after_clear=reports[1];
    et100_can_step(150);drain(false);assert(diagnostics.status==5);
    et100_can_step(200);drain(false);assert(diagnostics.status==5&&invalidations==1);
    et100_can_error_isr(true,true,255,128);et100_can_step(250);drain(false);
    assert(diagnostics.status==4&&invalidations==1&&reports[1]==after_clear);
    hardware_off=false;et100_can_step(350);et100_can_step(400);drain(false);
    assert(diagnostics.status==2&&invalidations==1&&reports[1]==after_clear);
    /* Configuration snapshots remain immediate even with no received frame. */
    reset();et100_can_step(0);drain(false);
    request("CAN$SGTIME;42;ABCD",50);
    assert(strstr(collected,"AT$PD=67;1;000000;;;")&&invalidations==0);
    puts("PASS quiet CAN: 30s scan, one invalidate per RX epoch, failure/BUS_OFF and config snapshots");
    puts("PASS ET100 CAN glue: queue/ring, config readback/defaults, scan/lock, SYS bus-off, pause/TTL and Type0");
    return 0;
}
