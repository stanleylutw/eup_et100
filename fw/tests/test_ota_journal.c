#include "ota_journal.h"
#include "fake_nor.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct fake_nor f, saved;
static struct ota_journal journal;
static struct ota_manifest manifest;
static uint8_t package[OTA_JOURNAL_ET100_MAX], acknowledged[64];
static uint8_t attempted[64];
static uint64_t cases, passed;
static unsigned losses;
static jmp_buf outage;

static int hash_nor(struct nor_port *nor, uint32_t a, uint32_t n, uint8_t digest[32], void *ctx)
{
    (void)ctx;
    mbedtls_sha256_context hash;uint8_t b[256];
    mbedtls_sha256_init(&hash);
    int rc=mbedtls_sha256_starts_ret(&hash,0);
    while (!rc && n) {
        size_t count=n<sizeof b ? n : sizeof b;
        rc=nor->read(a,b,count);
        if (!rc) rc=mbedtls_sha256_update_ret(&hash,b,count);
        a+=count;n-=count;
    }
    if (!rc) rc=mbedtls_sha256_finish_ret(&hash,digest);
    mbedtls_sha256_free(&hash);return rc;
}
static void setup(uint32_t n, uint32_t target)
{
    fake_nor_clear(&f);fake_nor_select(&f);memset(acknowledged,0,64);
    memset(attempted,0,64);
    memset(&journal,0xa5,sizeof journal);
    manifest=(struct ota_manifest){.session_id=123,.target=target,.source=OTA_RS232,.length=n};
    for (unsigned i=0;i<n;i++) package[i]=(uint8_t)((i*73u)^(i>>8)^(i>>17));
    /* An all-FF chunk must still have its own committed bitmap bit. */
    if (n>=4096) memset(package,0xff,4096);
    assert(mbedtls_sha256_ret(package,n,manifest.sha256,0)==0);
}
static void check(void)
{
    struct ota_progress p;assert(ota_journal_query(&journal,&p)==0);
    unsigned retransmit=0;
    for (unsigned k=0;k<512;k++)
        if ((attempted[k/8]&(1u<<(k%8))) && !(p.bitmap[k/8]&(1u<<(k%8)))) retransmit++;
    assert(retransmit<=1); /* Excludes future chunks never attempted. */
    unsigned count=(manifest.length+4095)/4096;
    if (p.state==OTA_NONE) {
        for (unsigned k=0;k<64;k++) assert(!acknowledged[k]);
        return;
    }
    assert(p.manifest.session_id==manifest.session_id);
    assert(p.manifest.length==manifest.length && p.manifest.target==manifest.target);
    for (unsigned k=0;k<64;k++) assert((p.bitmap[k]&acknowledged[k])==acknowledged[k]);
    uint32_t base=manifest.target==OTA_ET100 ? 0x20000 : 0x420000;
    for (unsigned k=0;k<count;k++) if (p.bitmap[k/8]&(1u<<(k%8))) {
        unsigned n=manifest.length-k*4096;n=n<4096 ? n : 4096;
        assert(!f.weak[base/4096+k]);
        assert(!memcmp(f.bytes+base+k*4096,package+k*4096,n));
    }
}
static void recover(void)
{
    memset(&journal,0,sizeof journal); /* Simulated reboot keeps no module RAM. */
    assert(ota_journal_recover(&journal,&f.port,hash_nor,NULL)==0);check();
    /* A committed record can survive even when its ACK was lost. Preserve
     * recovered progress too, including across another immediate outage.
     */
    memcpy(acknowledged,journal.progress.bitmap,64);
    memcpy(attempted,journal.progress.bitmap,64);
}
static int tracked_write(unsigned index)
{
    memcpy(attempted,acknowledged,64);attempted[index/8]|=(uint8_t)(1u<<(index%8));
    unsigned n=manifest.length-index*4096;n=n<4096 ? n : 4096;
    int rc=ota_journal_write_chunk(&journal,manifest.session_id,index,package+index*4096,n);
    if (!rc) memcpy(acknowledged,journal.progress.bitmap,64);
    return rc;
}
static void takeover(void)
{
    struct ota_manifest m=manifest;m.source=OTA_BLE;
    uint8_t before[64];memcpy(before,journal.progress.bitmap,64);
    assert(ota_journal_resume(&journal,&m)==0);
    assert(!memcmp(before,journal.progress.bitmap,64) && journal.progress.manifest.source==OTA_BLE);
}
static void continue_download(void)
{
    recover();
    if (journal.progress.state==OTA_NONE) assert(ota_journal_begin(&journal,&manifest)==0);
    for (unsigned i=0;i<(manifest.length+4095)/4096;i++) {
        if (i==8) takeover();
        if (journal.progress.bitmap[i/8]&(1u<<(i%8))) continue;
        assert(tracked_write(i)==0);
        memcpy(acknowledged,journal.progress.bitmap,64);
    }
    assert(ota_journal_finish(&journal,manifest.session_id)==0);
    assert(journal.progress.state==OTA_VERIFIED);check();
    uint8_t actual[32];assert(hash_nor(&f.port,manifest.target==OTA_ET100 ? 0x20000 : 0x420000,
                                     manifest.length,actual,NULL)==0);
    assert(!memcmp(actual,manifest.sha256,32));
}
static void rules(void)
{
    setup(4096+17,OTA_SENSOR);recover();
    struct ota_manifest invalid=manifest;invalid.length=OTA_JOURNAL_SENSOR_MAX+1;
    assert(ota_journal_begin(&journal,&invalid)==NOR_EINVAL);
    invalid.target=OTA_ET100;invalid.length=OTA_JOURNAL_ET100_MAX+1;
    assert(ota_journal_begin(&journal,&invalid)==NOR_EINVAL);
    invalid=manifest;invalid.length=0;assert(ota_journal_begin(&journal,&invalid)==NOR_EINVAL);
    assert(ota_journal_begin(&journal,&manifest)==0);
    uint64_t io=f.operations;
    assert(ota_journal_write_chunk(&journal,124,0,package,4096)==OTA_ESESSION);
    assert(ota_journal_finish(&journal,124)==OTA_ESESSION);
    assert(ota_journal_cancel(&journal,124)==OTA_ESESSION);
    assert(f.operations==io);
    for (unsigned mismatch=0;mismatch<4;mismatch++) {
        struct ota_manifest other=manifest;
        if (mismatch==0) other.session_id++;
        if (mismatch==1) other.length++;
        if (mismatch==2) other.sha256[0]^=1;
        if (mismatch==3) other.target=OTA_ET100;
        assert(ota_journal_resume(&journal,&other)==OTA_ESESSION && f.operations==io);
    }
    assert(ota_journal_resume(&journal,&manifest)==0 && f.operations==io);
    takeover();recover();assert(journal.progress.manifest.source==OTA_BLE);
    assert(ota_journal_begin(&journal,&manifest)==OTA_EBUSY);
    assert(ota_journal_finish(&journal,manifest.session_id)==OTA_EINCOMPLETE);
    assert(ota_journal_write_chunk(&journal,manifest.session_id,2,package,4096)==NOR_ERANGE);
    assert(ota_journal_write_chunk(&journal,manifest.session_id,1,package+4096,16)==NOR_EINVAL);
    assert(ota_journal_write_chunk(&journal,manifest.session_id,0,package,4096)==0);
    uint64_t before=f.programs;
    assert(ota_journal_write_chunk(&journal,manifest.session_id,0,package,4096)==0 && f.programs==before);
    package[0]^=1;assert(ota_journal_write_chunk(&journal,manifest.session_id,0,package,4096)==OTA_ECONTENT);package[0]^=1;
    assert(ota_journal_write_chunk(&journal,manifest.session_id,1,package+4096,17)==0);
    assert(ota_journal_finish(&journal,manifest.session_id)==0 && journal.progress.state==OTA_VERIFIED);
    io=f.operations;
    assert(ota_journal_write_chunk(&journal,124,0,package,4096)==OTA_ESESSION);
    assert(ota_journal_finish(&journal,124)==OTA_ESESSION);
    assert(ota_journal_cancel(&journal,124)==OTA_ESESSION && f.operations==io);
    before=f.programs;
    assert(ota_journal_write_chunk(&journal,manifest.session_id,0,package,4096)==0 && f.programs==before);
    package[0]^=1;assert(ota_journal_write_chunk(&journal,manifest.session_id,0,package,4096)==OTA_ECONTENT);package[0]^=1;
    assert(ota_journal_begin(&journal,&manifest)==OTA_EBUSY);
    assert(ota_journal_cancel(&journal,manifest.session_id)==0);
    memset(acknowledged,0,64);memset(attempted,0,64);recover();
    assert(journal.progress.state==OTA_CANCELLED);
    assert(ota_journal_write_chunk(&journal,manifest.session_id,0,package,4096)==OTA_ESTATE);
    assert(ota_journal_finish(&journal,manifest.session_id)==OTA_ESTATE);
    assert(ota_journal_begin(&journal,&manifest)==0);
    assert(ota_journal_write_chunk(&journal,manifest.session_id,0,package,4096)==0);
    assert(ota_journal_write_chunk(&journal,manifest.session_id,1,package+4096,17)==0);
    f.bytes[0x420001]^=1;
    assert(ota_journal_finish(&journal,manifest.session_id)==OTA_EHASH);
    assert(ota_journal_recover(&journal,&f.port,hash_nor,NULL)==0);
    assert(journal.progress.state==OTA_FAILED);
    setup(OTA_JOURNAL_SENSOR_MAX,OTA_SENSOR);continue_download();
    puts("PASS rules: bounds/55 sensor chunks/tail FF/duplicate/cancel/busy/hash failure/all-FF chunk");
    puts("PASS M1: wrong-session write/finish/cancel and resume identity mismatch cause zero NOR access; takeover persists; VERIFIED blocks begin");
}
static void exhaustive(void)
{
    setup(16*4096,OTA_ET100);continue_download();
    uint64_t operations=f.operations;
    /* Count the exact baseline, including final independent hash read. */
    cases=passed=0;
    for (unsigned mode=0;mode<3;mode++) for (uint64_t at=1;at<=operations;at++) {
        setup(16*4096,OTA_ET100);
        fake_nor_inject(&f,at,mode,&outage);
        if (!setjmp(outage)) continue_download();
        else { assert(f.cuts==1);fake_nor_inject(&f,0,mode,NULL);continue_download(); }
        assert(f.cuts==1);cases++;passed++;
        if (!(cases%3000)) printf("PROGRESS exhaustive %llu\n",(unsigned long long)cases);
    }
    printf("PASS exhaustive: operations=%llu modes=3 injections=%llu pass=%llu retransmit<=1 chunk (4096 B)\n",
           (unsigned long long)operations,(unsigned long long)cases,(unsigned long long)passed);
}
static uint32_t random_state=0x20261022;
static uint32_t random32(void)
{
    random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;
    return random_state;
}
static void randomized(void)
{
    setup(OTA_JOURNAL_ET100_MAX,OTA_ET100);losses=0;
    recover();assert(ota_journal_begin(&journal,&manifest)==0);
    while (losses<10000) {
        unsigned index=random32()%512;
        fake_nor_inject(&f,f.operations+1+random32()%800,random32()%3,&outage);
        if (!setjmp(outage)) {
            recover();
            takeover();assert(tracked_write(index)==0);
            memcpy(acknowledged,journal.progress.bitmap,64);
            fake_nor_inject(&f,0,0,NULL);
            bool complete=true;
            for (unsigned i=0;i<64;i++) if (journal.progress.bitmap[i]!=0xff) complete=false;
            if (complete) {
                assert(ota_journal_finish(&journal,manifest.session_id)==0);
                assert(ota_journal_cancel(&journal,manifest.session_id)==0);
                memset(acknowledged,0,64);memset(attempted,0,64);
                assert(ota_journal_begin(&journal,&manifest)==0);
            }
        } else {
            losses++;fake_nor_inject(&f,0,0,NULL);recover();
            if (!(losses%1000)) printf("PROGRESS random %u\n",losses);
        }
    }
    continue_download();
    printf("PASS random: seed=20261022 package=2097152 injections=%u pass=%u final SHA PASS\n",losses,losses);
}
static void gc_matrix(void)
{
    setup(16*4096,OTA_ET100);recover();assert(ota_journal_begin(&journal,&manifest)==0);
    for (unsigned i=0;i<14;i++) assert(tracked_write(i)==0);
    assert(journal.next_slot==16);memcpy(acknowledged,journal.progress.bitmap,64);
    fake_nor_copy(&saved,&f);
    uint8_t prior[64];memcpy(prior,acknowledged,64);
    f.operations=0;recover();takeover();assert(tracked_write(14)==0);
    uint64_t operations=f.operations;cases=passed=0;
    for (unsigned mode=0;mode<3;mode++) for (uint64_t at=1;at<=operations;at++) {
        fake_nor_copy(&f,&saved);fake_nor_select(&f);memcpy(acknowledged,prior,64);
        memcpy(attempted,prior,64);
        fake_nor_inject(&f,at,mode,&outage);
        if (!setjmp(outage)) { recover();takeover();assert(tracked_write(14)==0); }
        else { assert(f.cuts==1); }
        fake_nor_inject(&f,0,0,NULL);continue_download();cases++;passed++;
    }
    printf("PASS GC matrix: operations=%llu injections=%llu pass=%llu snapshot-before-old-erase\n",
           (unsigned long long)operations,(unsigned long long)cases,(unsigned long long)passed);
    for (unsigned mode=0;mode<3;mode++) {
        fake_nor_copy(&f,&saved);fake_nor_select(&f);memcpy(acknowledged,prior,64);
        memcpy(attempted,prior,64);
        losses=0;
        while (losses<3) {
            /* Reset inside recovery, then inside replacement header/snapshot work. */
            fake_nor_inject(&f,f.operations+(losses==0 ? 1 : losses==1 ? 70 : 100),mode,&outage);
            if (!setjmp(outage)) { continue_download();assert(!"expected nested cut"); }
            else losses++;
        }
        fake_nor_inject(&f,0,0,NULL);continue_download();
    }
    puts("PASS consecutive: 3 modes x 3 successive recovery/GC cuts = 9 injections/9 pass");
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);
    fake_nor_create(&f);fake_nor_create(&saved);
    rules();exhaustive();gc_matrix();randomized();
    fake_nor_destroy(&saved);fake_nor_destroy(&f);
    puts("M-HOST PASS (host simulation only; no EVB access)");return 0;
}
