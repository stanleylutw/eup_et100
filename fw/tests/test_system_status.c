#include "system_status.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char line[SYS_LINE_MAX+1];
static uint8_t values[SYS_COUNT];
static unsigned crc(const char *s, size_t n)
{
    unsigned c=0;
    while (n--) {
        c^=(unsigned char)*s++<<8;
        for (unsigned i=0;i<8;i++) c=((c<<1)^((c&0x8000)?0x1021:0))&0xffff;
    }
    return c;
}
static void check(size_t n, unsigned count)
{
    assert(n && n<=SYS_LINE_MAX && n==strlen(line));
    assert(!strcmp(line+n-2,"\r\n"));
    const char *star=strchr(line,'*'); assert(star && star==line+n-7);
    unsigned got;assert(sscanf(star+1,"%4X",&got)==1 && got==crc(line,(size_t)(star-line)));
    unsigned pairs=0;
    for (const char *s=line;*s;s++) pairs+=*s==',';
    assert(pairs==count);
}
int main(int argc, char **argv)
{
    if (argc==2 && !strcmp(argv[1],"--vector")) {
        et100_sys_init();et100_sys_set(SYS_NOR,2);
        assert(et100_sys_query("C001",0,line,sizeof line));
        fputs(line,stdout);return 0;
    }
    size_t n=et100_sys_query("A002",0,line,sizeof line);check(n,0);
    assert(strstr(line,";0;A002;2*"));
    et100_sys_init(); et100_sys_snapshot(values);
    assert(values[4]==SYS_UNSUPPORTED && values[29]==SYS_UNKNOWN);
    for (unsigned i=1;i<=SYS_COUNT;i++) {
        assert(et100_sys_valid(i,0));
        for (unsigned s=240;s<=244;s++) assert(et100_sys_valid(i,s));
        assert(!et100_sys_valid(i,239) && !et100_sys_valid(i,255));
    }
    assert(!et100_sys_valid(0,2) && !et100_sys_valid(34,2));
    et100_sys_set(0,2);et100_sys_set(34,2);et100_sys_set(3,239);
    et100_sys_set(3,2);
    et100_sys_ble(SYS_BLE_SCAN,true);et100_sys_snapshot(values);assert(values[3]==3);
    et100_sys_ble(SYS_BLE_ADV,true);et100_sys_snapshot(values);assert(values[3]==6);
    et100_sys_ble(SYS_BLE_LINK,true);et100_sys_ble(SYS_BLE_SCAN,false);
    et100_sys_ble(SYS_BLE_ADV,false);et100_sys_snapshot(values);assert(values[3]==5);
    et100_sys_ble(SYS_BLE_LINK,false);et100_sys_snapshot(values);assert(values[3]==2);
    et100_sys_boot_trial(true);
    n=et100_sys_poll(100,false,line,sizeof line);check(n,0);
    assert(!strcmp(line,"ET100$SYS;2*00F7\r\n")); /* verified below by CRC */
    assert(!et100_sys_poll(599,false,line,sizeof line));
    assert(!et100_sys_poll(4999,false,line,sizeof line));
    n=et100_sys_poll(5000,false,line,sizeof line);check(n,33);
    assert(strstr(line,"ET100$SYS;10;01,") && strstr(line,";33,242*"));
    assert(!et100_sys_poll(5500,false,line,sizeof line));
    et100_sys_set(3,5);et100_sys_set(3,2); /* coalesced back to already reported */
    assert(!et100_sys_poll(5501,false,line,sizeof line));
    et100_sys_set(3,4);
    n=et100_sys_poll(5502,false,line,sizeof line);check(n,1);assert(strstr(line,";11;03,4*"));
    et100_sys_set(3,2);assert(!et100_sys_poll(5999,false,line,sizeof line));
    assert(!et100_sys_poll(6000,true,line,sizeof line));
    n=et100_sys_poll(6500,false,line,sizeof line);check(n,33);
    n=et100_sys_query("Z9Q1",0,line,sizeof line);check(n,33);assert(strstr(line,";1;Z9Q1;10;"));
    assert(!et100_sys_query("bad!",0,line,sizeof line));
    assert(!et100_sys_query("abcd",0,line,sizeof line));
    assert(!et100_sys_query("A001",5,line,sizeof line));
    n=et100_sys_query("A001",1,line,sizeof line);check(n,0);assert(strstr(line,";0;A001;1*"));
    for (unsigned i=1;i<=SYS_COUNT;i++) et100_sys_set(i,244);
    n=et100_sys_query("A001",0,line,sizeof line);check(n,33);
    size_t exact=n;
    for (size_t cap=0;cap<=exact;cap++) {
        char small[SYS_LINE_MAX+2];memset(small,0x55,sizeof small);
        assert(!et100_sys_query("A001",0,small,cap));
        assert(small[cap]==0x55);
    }
    et100_sys_init();assert(!et100_sys_poll(0,false,line,1));
    n=et100_sys_poll(0,false,line,sizeof line);check(n,0);assert(strstr(line,";0*"));
    assert(!et100_sys_poll(100,true,line,sizeof line));
    n=et100_sys_poll(UINT32_MAX-1000,false,line,sizeof line);check(n,33);
    et100_sys_set(1,2);
    n=et100_sys_poll(0,false,line,sizeof line);check(n,1); /* unsigned tick wrap */
    puts("PASS SYS: registry, RAM snapshot, CRC, token, bounds, coalescing, throttle, bulk, BLE roles, wrap");
    return 0;
}
