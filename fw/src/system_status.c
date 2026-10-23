#include "system_status.h"
#include <stdio.h>
#include <string.h>
#ifndef ET100_SYS_HOST_TEST
#include "FreeRTOS.h"
#include "task.h"
#else
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif

static uint8_t current[SYS_COUNT], reported[SYS_COUNT];
static bool initialized, boot_pending, full_pending, emitted;
static uint8_t boot_type, ble_roles;
static uint32_t last_emit;
static const uint8_t limits[SYS_COUNT] = {
    3,4,5,6,7,5,7,6,6,6,5,6,6,6,6,4,3,4,4,4,5,5,6,5,5,5,5,4,4,6,3,3,3
};

bool et100_sys_valid(unsigned index, unsigned status)
{
    return index && index <= SYS_COUNT &&
        (status <= limits[index-1] || (status >= SYS_DISABLED && status <= SYS_ERROR));
}
void et100_sys_init(void)
{
    taskENTER_CRITICAL();
    memset(current, SYS_UNSUPPORTED, sizeof current);
    current[SYS_SYSTEM-1]=1; current[SYS_SETTINGS-1]=1;
    current[SYS_NOR-1]=1; current[SYS_BLE-1]=1; current[SYS_RESET-1]=SYS_UNKNOWN;
    memset(reported, SYS_UNKNOWN, sizeof reported);
    initialized=true; boot_pending=full_pending=true; emitted=false;
    ble_roles=boot_type=0; last_emit=0;
    taskEXIT_CRITICAL();
}
void et100_sys_set(unsigned index, unsigned status)
{
    if (!et100_sys_valid(index,status)) return;
    taskENTER_CRITICAL();
    if (initialized) current[index-1]=(uint8_t)status;
    taskEXIT_CRITICAL();
}
void et100_sys_snapshot(uint8_t out[SYS_COUNT])
{
    taskENTER_CRITICAL(); memcpy(out,current,SYS_COUNT); taskEXIT_CRITICAL();
}
void et100_sys_ble(unsigned role, bool active)
{
    if (!role || (role & ~(SYS_BLE_SCAN|SYS_BLE_ADV|SYS_BLE_LINK))) return;
    taskENTER_CRITICAL();
    if (active) ble_roles |= (uint8_t)role;
    else ble_roles &= (uint8_t)~role;
    unsigned roles=ble_roles;
    if (initialized) current[SYS_BLE-1]=!roles ? 2 : (roles & (roles-1)) ? 6 :
        roles==SYS_BLE_SCAN ? 3 : roles==SYS_BLE_ADV ? 4 : 5;
    taskEXIT_CRITICAL();
}
void et100_sys_boot_trial(bool trial)
{
    if (boot_pending) boot_type=trial ? 2 : 0;
}

static uint16_t crc16(const char *text, size_t n)
{
    uint16_t crc=0;
    for (size_t i=0;i<n;i++) {
        crc ^= (uint16_t)(uint8_t)text[i]<<8;
        for (unsigned b=0;b<8;b++) crc=(uint16_t)((crc<<1)^((crc&0x8000)?0x1021:0));
    }
    return crc;
}
static size_t finish(char *out, size_t cap, size_t n)
{
    if (n+7 >= cap || n+7 > SYS_LINE_MAX) { if (cap) out[0]=0; return 0; }
    int tail=snprintf(out+n,cap-n,"*%04X\r\n",(unsigned)crc16(out,n));
    return tail==7 ? n+7 : 0;
}
static size_t format(const uint8_t values[SYS_COUNT], unsigned type, const char *token,
                     bool full, char *out, size_t cap)
{
    if (!cap) return 0;
    int prefix=token ? snprintf(out,cap,"AT$PD=BLEGW$GSYSSTAT;1;%s;10",token) :
        snprintf(out,cap,"ET100$SYS;%u",type);
    if (prefix<0 || (size_t)prefix>=cap) { out[0]=0;return 0; }
    size_t n=(size_t)prefix;
    for (unsigned i=0;i<SYS_COUNT;i++) {
        if (!full && values[i]==reported[i]) continue;
        int add=snprintf(out+n,cap-n,";%02u,%u",i+1,values[i]);
        if (add<0 || (size_t)add>=cap-n) { out[0]=0;return 0; }
        n+=(size_t)add;
    }
    return finish(out,cap,n);
}
size_t et100_sys_query(const char *token, unsigned error, char *out, size_t cap)
{
    if (!token || strlen(token)!=4 || !cap || error>4) return 0;
    for (unsigned i=0;i<4;i++)
        if (!((token[i]>='A' && token[i]<='Z') || (token[i]>='0' && token[i]<='9'))) return 0;
    if (!initialized && !error) error=2;
    if (error) {
        int n=snprintf(out,cap,"AT$PD=BLEGW$GSYSSTAT;0;%s;%u",token,error);
        if (n<0 || (size_t)n>=cap) { out[0]=0;return 0; }
        return finish(out,cap,(size_t)n);
    }
    uint8_t values[SYS_COUNT];et100_sys_snapshot(values);
    return format(values,10,token,true,out,cap);
}
size_t et100_sys_poll(uint32_t now, bool paused, char *out, size_t cap)
{
    if (!initialized) return 0;
    if (paused) { full_pending=true;return 0; }
    if (emitted && (uint32_t)(now-last_emit)<500) return 0;
    size_t n=0;
    if (boot_pending) {
        if (!cap) return 0;
        int prefix=snprintf(out,cap,"ET100$SYS;%u",boot_type);
        if (prefix>=0 && (size_t)prefix<cap) n=finish(out,cap,(size_t)prefix);
        if (n) boot_pending=false;
    } else {
        /* Initial collection is five seconds from App monotonic time. */
        if (full_pending && now<5000 && last_emit<5000) return 0;
        uint8_t values[SYS_COUNT];et100_sys_snapshot(values);
        if (!full_pending && !memcmp(values,reported,SYS_COUNT)) return 0;
        n=format(values,full_pending ? 10 : 11,NULL,full_pending,out,cap);
        if (n) { memcpy(reported,values,SYS_COUNT);full_pending=false; }
    }
    if (n) { emitted=true;last_emit=now; }
    return n;
}
