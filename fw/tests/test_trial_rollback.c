#include "transaction.h"
#include "integrity.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define SIZE 1576
static uint8_t flash[0x200000],nor[0x460000],saved_flash[sizeof flash],saved_nor[sizeof nor];
static uint8_t used[sizeof nor/8],saved_used[sizeof used];
static bool weak[sizeof nor/4096],saved_weak[sizeof weak];
static uint8_t a[SIZE],b[SIZE],ha[32],hb[32];
static unsigned operations,cut,mode,nor_writes;
static bool backup_error;
static jmp_buf outage;
static void put32(uint8_t *p,uint32_t v) { for (unsigned i=0;i<4;i++) p[i]=v>>(8*i); }
static void fixture(uint8_t image[SIZE],unsigned version,uint8_t hash[32])
{
    memset(image,0,SIZE);put32(image,0x96f3b83d);image[9]=2;put32(image+12,1024);
    put32(image+512,0x20001000);put32(image+516,0x08020209);image[530]=version;
    uint8_t *m=image+576;memcpy(m,"EUPFWV1",8);m[8]=1;
    memcpy(m+10,"EUP_ET100",10);memcpy(m+34,"ET100_EVB_DEV1.4",16);
    put32(m+282,installer_crc32(m,282));
    memcpy(image+SIZE-40,"\x07\x69\x28\0\x10\0\x20\0",8);
    assert(!mbedtls_sha256_ret(image,SIZE-40,image+SIZE-32,0));
    assert(!mbedtls_sha256_ret(image,SIZE,hash,0));
}
static bool interrupted(void) { return ++operations==cut; }
static size_t amount(size_t n,bool fail) { return !fail || mode==2 ? n : mode==1 ? n/2 : 0; }
static int rd(uint32_t off,void *data,size_t n)
{ assert(off<=sizeof flash && n<=sizeof flash-off);memcpy(data,flash+off,n);return 0; }
static bool writable(uint32_t off,size_t n)
{ return (off>=0x20000 && off<0x1e0000 && n<=0x1e0000-off) ||
         (off>=BOOT_STATE_BASE && off<sizeof flash && n<=sizeof flash-off); }
static int er(uint32_t off,size_t n)
{
    assert(writable(off,n) && !(off&4095) && !(n&4095));bool fail=interrupted();
    memset(flash+off,0xff,amount(n,fail));if (fail) longjmp(outage,1);return 0;
}
static int wr(uint32_t off,const void *data,size_t n)
{
    assert(writable(off,n));bool fail=interrupted();const uint8_t *p=data;
    for (size_t i=0;i<amount(n,fail);i++) { assert((flash[off+i]&p[i])==p[i]);flash[off+i]&=p[i]; }
    if (fail) longjmp(outage,1);return 0;
}
static const struct internal_port port={rd,er,wr};
static int nr(uint32_t off,void *data,size_t n)
{
    assert(off<=sizeof nor && n<=sizeof nor-off);memcpy(data,nor+off,n);
    for (size_t i=0;i<n;i++) if (weak[(off+i)/4096]) { ((uint8_t *)data)[i]^=1;break; }
    return 0;
}
static int ne(uint32_t off,uint32_t n)
{
    assert(off>=0x220000 && off<0x420000 && n==4096 && !(off&4095));
    if (backup_error) return NOR_ETIMEOUT;
    bool fail=interrupted();nor_writes++;memset(nor+off,0xff,amount(n,fail));
    weak[off/4096]=fail;
    if (fail) longjmp(outage,1);
    memset(used+off/8,0,n/8);return 0;
}
static int nw(uint32_t off,const void *data,size_t n)
{
    assert(off>=0x220000 && off<0x420000 && n<=0x420000-off);
    const uint8_t *p=data;
    for (size_t i=0;i<n;) {
        unsigned count=n-i<8 ? n-i : 8;bool fail=interrupted();nor_writes++;
        assert(!weak[(off+i)/4096] && !used[(off+i)/8]);used[(off+i)/8]=1;
        for (size_t k=0;k<amount(count,fail);k++) {
            assert((nor[off+i+k]&p[i+k])==p[i+k]);nor[off+i+k]&=p[i+k];
        }
        if (fail) longjmp(outage,1);i+=count;
    }
    return 0;
}
static struct nor_port external={.read=nr,.erase=ne,.program=nw};
static void save(void)
{
    memcpy(saved_flash,flash,sizeof flash);memcpy(saved_nor,nor,sizeof nor);
    memcpy(saved_used,used,sizeof used);memcpy(saved_weak,weak,sizeof weak);
}
static void restore(void)
{
    memcpy(flash,saved_flash,sizeof flash);memcpy(nor,saved_nor,sizeof nor);
    memcpy(used,saved_used,sizeof used);memcpy(weak,saved_weak,sizeof weak);
    operations=0;cut=0;nor_writes=0;
}
static void protected(void)
{
    assert(!memcmp(flash,saved_flash,0x20000));
    assert(!memcmp(flash+0x1e0000,saved_flash+0x1e0000,0x1c000));
    assert(!memcmp(nor,saved_nor,0x220000));
    assert(!memcmp(nor+0x420000,saved_nor+0x420000,0x40000));
}
static struct boot_record record;
static void read_state(void) { memset(&record,0xa5,sizeof record);assert(!boot_state_read(&port,&record)); }
static void run(void) { assert(!transaction_run(&port,&external,&record,NULL)); }
static unsigned matrix(unsigned scenario)
{
    save();operations=0;
    if (scenario==1) assert(!boot_state_confirm(&port,hb));else run();
    unsigned count=operations;
    for (mode=0;mode<3;mode++) for (unsigned point=1;point<=count;point++) {
        restore();read_state();cut=point;
        if (!setjmp(outage)) {
            if (scenario==1) (void)boot_state_confirm(&port,hb);else run();
            assert(0 && "every selected mutation must cut");
        }
        cut=0;read_state();
        if (record.kind!=BOOT_CONFIRMED) run();
        read_state();
        if (record.kind==BOOT_TRIAL) {
            assert(!memcmp(flash+0x20000,b,SIZE));assert(!memcmp(record.hash,hb,32));
        } else {
            assert(record.kind==BOOT_CONFIRMED);
            const uint8_t *expected=memcmp(record.confirmed_hash,ha,32) ? b : a;
            assert(!memcmp(flash+0x20000,expected,SIZE));
            if (scenario==2) assert(expected==a && record.last_update==UPDATE_FAILED);
        }
        protected();
    }
    restore();read_state();
    if (scenario==1) assert(!boot_state_confirm(&port,hb));else run();read_state();
    return count*3;
}
int main(void)
{
    fixture(a,1,ha);fixture(b,2,hb);memset(flash,0xa5,sizeof flash);memset(nor,0xff,sizeof nor);
    memset(flash+BOOT_STATE_BASE,0xff,16384);memcpy(flash+0x20000,a,SIZE);memcpy(nor+0x20000,b,SIZE);
    /* Import actual v0.8.3 wire format and promote it without erasing the old bank. */
    uint8_t legacy[60]={0};put32(legacy,0x31545342);put32(legacy+4,5);put32(legacy+8,2);
    put32(legacy+12,SIZE);memcpy(legacy+20,ha,32);put32(legacy+52,installer_crc32(legacy,52));
    put32(legacy+56,0x54494d43);memcpy(flash+BOOT_STATE_BASE,legacy,60);read_state();
    assert(record.kind==BOOT_CONFIRMED && record.confirmed_length==SIZE && !memcmp(record.confirmed_hash,ha,32));
    record.kind=BOOT_PENDING;record.length=SIZE;memcpy(record.hash,hb,32);assert(!boot_state_write(&port,&record));
    unsigned previous=operations;assert(boot_state_confirm(&port,hb));assert(operations==previous);
    read_state();backup_error=true;run();read_state();
    assert(record.kind==BOOT_CONFIRMED && record.last_update==UPDATE_ABORTED && !memcmp(flash+0x20000,a,SIZE));
    backup_error=false;record.kind=BOOT_PENDING;assert(!boot_state_write(&port,&record));read_state();
    unsigned cases=matrix(0);assert(record.kind==BOOT_TRIAL);
    operations=0;assert(boot_state_confirm(&port,ha));assert(!operations);
    /* Save trial for rollback matrix before independently testing confirmation. */
    struct boot_record trial=record;
    cases+=matrix(1);assert(record.kind==BOOT_CONFIRMED && !memcmp(record.confirmed_hash,hb,32));
    record=trial;assert(!boot_state_write(&port,&record));read_state();cases+=matrix(2);
    assert(record.kind==BOOT_CONFIRMED && !memcmp(flash+0x20000,a,SIZE));
    /* A retained, fully verified matching rollback image must not be rewritten. */
    record.kind=BOOT_PENDING;record.length=SIZE;memcpy(record.hash,hb,32);assert(!boot_state_write(&port,&record));
    read_state();nor_writes=0;run();assert(nor_writes==0);
    /* Corrupt rollback must fail before erasing the currently installed App. */
    nor[0x220000+576+282]^=1;
    assert(transaction_run(&port,&external,&record,NULL)!=0);read_state();
    assert(record.kind==BOOT_ROLLBACK_REQUIRED && !memcmp(flash+0x20000,b,SIZE));
    puts("PASS trial/confirm/rollback, v1 migration, backup abort/reuse, bad rollback and wrong-state/hash rejection");
    printf("PASS backup/TRIAL/confirm/rollback power-cut matrix (%u cases, three torn-operation modes)\n",cases);
}
