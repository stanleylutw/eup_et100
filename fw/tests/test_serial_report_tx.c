#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "serial_report_tx.h"
#include "system_status.h"
#include "ble_reporter.h"

enum { SMP_BUSY=1, RX_PENDING=2, PARTIAL_PACKET=4 };
struct fixture {
    unsigned blocked, pause, inject_lock, inject_sys, locked, systems, beacons;
    uint32_t now;
    struct et100_reporter reporter;
    char line[SYS_LINE_MAX+1];
};

static bool take(void *context)
{
    struct fixture *f=context;
    assert(!f->locked);
    if (f->blocked) return false;
    f->locked=1;
    if (f->inject_lock) f->pause|=SMP_BUSY;
    return true;
}
static void give(void *context)
{ struct fixture *f=context;assert(f->locked);f->locked=0; }
static bool paused(bool system,void *context)
{
    struct fixture *f=context;assert(f->locked);
    if (system && f->pause) (void)et100_sys_poll(f->now,true,NULL,0);
    return f->pause!=0;
}
static void system_report(void *context)
{
    struct fixture *f=context;assert(f->locked && !f->pause);
    size_t n=et100_sys_poll(f->now,false,f->line,sizeof f->line);
    if (n) {
        f->systems++;
        /* SMP arrives while this already-claimed SYS line is being sent. */
        if (f->inject_sys) f->pause|=SMP_BUSY;
    }
}
static void beacon_report(void *context)
{
    struct fixture *f=context;assert(f->locked && !f->pause);
    size_t n=et100_reporter_poll(&f->reporter,f->now,f->line,sizeof f->line);
    if (n) f->beacons++;
}
int main(void)
{
    struct fixture f={.now=5000};
    const struct et100_report_tx tx={take,give,paused,&f};
    et100_sys_init();
    const uint16_t cid[4]={0x0639};
    const uint8_t mac[6]={6,5,4,3,2,1},ad[]={3,0xff,0x39,0x06};
    et100_reporter_init(&f.reporter,cid,1,0);
    assert(et100_reporter_receive(&f.reporter,mac,0,-50,ad,sizeof ad,0));
    struct et100_reporter before=f.reporter;

    /* Do not consume either producer when the SMP sender owns TX. */
    f.blocked=1;
    et100_report_tx_poll(&tx,true,system_report);
    et100_report_tx_poll(&tx,false,beacon_report);
    assert(!f.systems && !f.beacons && !memcmp(&before,&f.reporter,sizeof before));

    /* A queued SMP observed after taking TX must suppress this whole poll. */
    f.blocked=0;f.inject_lock=1;
    et100_report_tx_poll(&tx,true,system_report);
    et100_report_tx_poll(&tx,false,beacon_report);
    assert(!f.systems && !f.beacons && !f.locked);
    assert(!memcmp(&before,&f.reporter,sizeof before));

    /* M1: SYS is allowed to finish, but the following Beacon must recheck. */
    f.inject_lock=0;f.pause=0;f.inject_sys=1;
    et100_report_tx_poll(&tx,true,system_report);
    assert(f.systems==1 && strstr(f.line,"ET100$SYS;0*")==f.line);
    et100_report_tx_poll(&tx,false,beacon_report);
    assert(!f.beacons && !f.locked && !memcmp(&before,&f.reporter,sizeof before));

    /* Pending RX/half packets also defer rather than discard producer state. */
    for (unsigned reason=SMP_BUSY;reason<=PARTIAL_PACKET;reason<<=1) {
        f.pause=reason;
        et100_report_tx_poll(&tx,true,system_report);
        et100_report_tx_poll(&tx,false,beacon_report);
        assert(f.systems==1 && !f.beacons && !f.locked);
        assert(!memcmp(&before,&f.reporter,sizeof before));
    }

    f.pause=0;f.inject_sys=0;f.now=8000;
    et100_report_tx_poll(&tx,true,system_report);
    assert(f.systems==2 && strstr(f.line,"ET100$SYS;10;")==f.line);
    et100_report_tx_poll(&tx,false,beacon_report);
    assert(f.beacons==1 && strstr(f.line,"AT$PD=G1;1;010203040506;")==f.line && !f.locked);
    puts("PASS report TX arbitration: lock-before-gate, SMP during SYS suppresses next Beacon,");
    puts("     busy TX/RX/partial packets preserve real SYS and reporter state, deferred resume");
}
