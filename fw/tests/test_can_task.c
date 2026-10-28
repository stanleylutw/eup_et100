/* ET-100 glue tests: unchanged F407 modules + fake board/storage. No EVB. */
#define ET100_CAN_HOST_TEST
#include "../src/can_task.c"
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
static void drain(bool stop){char line[160];size_t n;collected[0]=0;
    while((n=et100_can_take_line(line,sizeof line,stop))){assert(strlen(collected)+n<sizeof collected);strcat(collected,line);}}
static void request(const char *line,uint32_t now){char fail[160];assert(!et100_can_command(line,fail,sizeof fail));et100_can_step(now);drain(false);}
static void reset(void){exists=fail_write=corrupt_read=fail_apply=hardware_off=false;writes=applies=0;et100_can_start(1);}
int main(void)
{
    reset();assert(rate==250&&applies==1&&states[11]==2);
    request("CAN$GVERSION;ABCD",0);assert(strstr(collected,"CAN$GVERSION;1;V1:97;ABCD*"));
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
    assert(strstr(collected,"AT$PD=67;0;V1;97;"));
    et100_can_step(300050);drain(false);assert(!strstr(collected,"AT$PD=67;0;"));
    et100_can_set_paused(true);et100_can_step(330000);drain(false);assert(!collected[0]);
    request("CAN$GVERSION;ABCD",330001);assert(strstr(collected,"CAN$GVERSION;1;V1:97;"));
    et100_can_set_paused(false);et100_can_step(335000);drain(false);assert(strstr(collected,"AT$PD=67;1;"));
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
    assert(strstr(collected,"CAN$GVERSION;1;V1:97;")&&!strstr(collected,"AT$PD=67;"));
    drain(false);assert(strstr(collected,"AT$PD=67;0;V1;97;"));
    drain(false);assert(!collected[0]);
    puts("PASS ET100 CAN glue: queue/ring, config readback/defaults, scan/lock, SYS bus-off, pause/TTL and Type0");
    return 0;
}
