#include "app_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct et100_manager table;
static struct et100_reporter reporter;
static const uint8_t mac[6]={0x1f,0x30,0xe9,0x30,0xdb,0xfa};
static const uint8_t ad[]={2,1,6,17,0xff,0x39,0x06,0x10,3,6,14,0x11,0,0x20,0,0,1,2,3,4,5};
static uint8_t out[ET100_MANAGER_LIST_MAX];
static uint32_t get32(const uint8_t *p)
{return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void receive(const uint8_t addr[6],uint32_t now)
{
    struct et100_adv_details info;
    assert(et100_reporter_receive_details(&reporter,addr,1,-50,ad,sizeof(ad),now,&info));
    assert(info.version==2 && info.mode==1 && info.state==2 && info.identity_ready);
    et100_manager_update(&table,addr,1,-50,0,ad,sizeof(ad),&info,now);
}
int main(void)
{
    uint16_t filters[4]={0x0639,0,0,0};
    et100_reporter_init(&reporter,filters,15,0);et100_manager_init(&table);
    assert(et100_manager_status(&table,0,out,sizeof(out))==16 && out[2]==0 && out[3]==0xff);
    assert(et100_manager_selected(&table,0,out,sizeof(out))==6 && out[1]==0xff);
    receive(mac,0);
    assert(et100_manager_list(&table,0,out,sizeof(out))==20);
    assert(!memcmp(out,(uint8_t[]){5,1,16,1,0,1},6));
    assert(!memcmp(out+6,mac,6) && out[12]==(uint8_t)-50);
    assert(!memcmp(out+13,(uint8_t[]){2,1,2,3,6,14,65},7));
    assert(et100_manager_select(&table,0,(uint8_t[]){1,1,0,0},4));
    assert(et100_manager_selected(&table,0,out,sizeof(out))==23+sizeof(ad));
    assert(out[1]==0 && get32(out+2)==1 && out[17]==67 && out[22]==sizeof(ad));
    assert(!memcmp(out+23,ad,sizeof(ad)) && !memcmp(out+8,mac,6));
    assert(out[15]==0xff && out[16]==0xff);
    assert(!et100_manager_selected(&table,0,out,22));
    uint8_t by_addr[10]={1,1,1,1};memcpy(by_addr+4,mac,6);
    assert(et100_manager_select(&table,0,by_addr,sizeof(by_addr)));
    assert(!et100_manager_select(&table,0,(uint8_t[]){1,1,0,8},4));
    assert(!et100_manager_select(&table,0,(uint8_t[]){1,2,0},3));
    assert(!et100_manager_select(&table,0,NULL,0));
    assert(table.selected==0);
    assert(et100_manager_status(&table,5000,out,sizeof(out))==16 && out[2]==1);
    assert(et100_manager_status(&table,5001,out,sizeof(out))==16 && out[2]==0 && out[3]==0xff);
    assert(!(table.entries[0].flags&2));
    receive(mac,6000);assert(table.selected==0xff);
    for(unsigned i=1;i<9;i++) {uint8_t next[6]={0};next[0]=(uint8_t)i;receive(next,6000);}
    assert(et100_manager_list(&table,6000,out,sizeof(out))==132 && out[1]==8);
    assert(et100_manager_list(&table,6000,out,19)==4 && out[1]==0);
    assert(et100_manager_select(&table,6000,(uint8_t[]){1,2},2));
    et100_manager_init(&table);receive(mac,UINT32_MAX-100);
    assert(et100_manager_status(&table,4899,out,sizeof(out))==16 && out[2]==1);
    assert(et100_manager_status(&table,4900,out,sizeof(out))==16 && out[2]==0);
    struct et100_adv_details info;
    uint8_t v1[]={20,0xff,0x39,0x06,1,2,3,4,5,6,0,0x11,0,0x20,0,0,1,2,3,4,5};
    assert(et100_reporter_receive_details(&reporter,mac,1,-50,v1,sizeof(v1),0,&info));
    assert(info.version==1 && !memcmp(info.identity,(uint8_t[]){1,2,3,4,5,6},6));
    memset(v1+4,0,6);
    assert(et100_reporter_receive_details(&reporter,mac,1,-50,v1,sizeof(v1),1,&info) && !info.identity_ready);
    uint8_t generic[]={3,0xff,0x39,0x06};
    assert(et100_reporter_receive_details(&reporter,mac,1,-50,generic,sizeof(generic),0,&info) && !info.version);
    uint8_t service_data[sizeof(ad)];memcpy(service_data,ad,sizeof(ad));service_data[4]=0x16;
    assert(et100_reporter_receive_details(&reporter,mac,1,-50,service_data,sizeof(service_data),0,&info));
    assert(info.version==2 && !memcmp(info.firmware,(uint8_t[]){3,6,14},3));
    et100_manager_init(&table);
    et100_manager_update(&table,mac,0,-50,0,ad,sizeof(ad),&info,0);
    et100_manager_update(&table,mac,1,-50,0,ad,sizeof(ad),&info,0);
    assert(et100_manager_list(&table,0,out,sizeof(out))==36 && out[1]==2);
    uint8_t bad[]={31,0xff};
    assert(!et100_reporter_receive_details(&reporter,mac,1,-50,bad,sizeof(bad),0,&info));
    puts("PASS: App status/list v5/selected vectors, byte order, bounds, select, timeout, eight slots, wrap, shared AD metadata");
    return 0;
}
