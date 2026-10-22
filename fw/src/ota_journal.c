#include "ota_journal.h"
#include <string.h>

#define SECTORS 32u
#define RECORD 256u
#define MAGIC 0x314c4e4au
#define HEADER 0x10001u
#define SNAPSHOT 0x10002u
static const uint8_t marker[8] = {'C','O','M','M','I','T','1','!'};

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint64_t get64(const uint8_t *p) { return get32(p) | (uint64_t)get32(p+4)<<32; }
static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(i*8));
}
static void put64(uint8_t *p, uint64_t v) { put32(p,(uint32_t)v); put32(p+4,(uint32_t)(v>>32)); }
static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t crc=UINT32_MAX;
    while (n--) {
        crc^=*p++;
        for (unsigned i=0;i<8;i++) crc=(crc>>1) ^ (0xedb88320u & (0u-(crc&1)));
    }
    return ~crc;
}
static uint32_t chunks(const struct ota_progress *p)
{
    return (p->manifest.length+4095u)/4096u;
}
static bool bit(const struct ota_progress *p, unsigned n) { return (p->bitmap[n/8]>>(n%8))&1; }
static uint32_t base(const struct ota_progress *p) { return p->manifest.target==OTA_ET100 ? 0x20000u : 0x420000u; }
static bool manifest_valid(const struct ota_manifest *m)
{
    uint32_t max=m->target==OTA_ET100 ? OTA_JOURNAL_ET100_MAX :
                 m->target==OTA_SENSOR ? OTA_JOURNAL_SENSOR_MAX : 0;
    return m->session_id && m->length && m->length<=max &&
           m->source>=OTA_LTE && m->source<=OTA_RS232;
}
static bool valid_record(const uint8_t *b, uint32_t type)
{
    return get32(b)==MAGIC && get32(b+4)==type && get64(b+8) &&
           !memcmp(b+248,marker,8) && get32(b+244)==crc32(b,244);
}
static bool decode(const uint8_t *b, struct ota_progress *p)
{
    memset(p,0,sizeof *p);
    p->manifest.session_id=get64(b+16); p->state=get32(b+24);
    p->manifest.target=get32(b+28); p->manifest.source=get32(b+32);
    p->manifest.length=get32(b+36);
    memcpy(p->manifest.sha256,b+40,32); memcpy(p->bitmap,b+72,64);
    if (p->state==OTA_NONE) {
        if (p->manifest.session_id || p->manifest.length || p->manifest.target || p->manifest.source)
            return false;
    } else if (p->state>OTA_CANCELLED || !manifest_valid(&p->manifest)) return false;
    for (unsigned n=0;n<512;n++) {
        if ((n>=chunks(p) || p->state==OTA_CANCELLED || p->state==OTA_NONE) && bit(p,n)) return false;
        if ((p->state==OTA_VERIFIED || p->state==OTA_FAILED) && n<chunks(p) && !bit(p,n)) return false;
    }
    return true;
}
static void encode(uint8_t b[256], uint32_t type, uint64_t seq, const struct ota_progress *p)
{
    memset(b,0xff,256); put32(b,MAGIC); put32(b+4,type); put64(b+8,seq);
    if (p) {
        put64(b+16,p->manifest.session_id); put32(b+24,p->state);
        put32(b+28,p->manifest.target); put32(b+32,p->manifest.source);
        put32(b+36,p->manifest.length); memcpy(b+40,p->manifest.sha256,32);
        memcpy(b+72,p->bitmap,64);
    }
    put32(b+244,crc32(b,244)); memcpy(b+248,marker,8);
}
static int fail(struct ota_journal *j, int rc) { if (rc) j->ready=false; return rc; }
static int read_at(struct ota_journal *j, uint32_t a, void *p, size_t n)
{
    return fail(j,j->nor->read(a,p,n));
}
static int write_record(struct ota_journal *j, uint32_t a, const uint8_t b[256])
{
    uint8_t verify[256];
    for (unsigned pos=0;pos<248;pos+=8) {
        int rc=j->nor->program(a+pos,b+pos,8);
        if (rc) return fail(j,rc);
    }
    int rc=read_at(j,a,verify,248);
    if (rc) return rc;
    if (memcmp(b,verify,248)) return fail(j,NOR_EVERIFY);
    if ((rc=j->nor->program(a+248,b+248,8))) return fail(j,rc);
    if ((rc=read_at(j,a,verify,256))) return rc;
    return memcmp(b,verify,256) ? fail(j,NOR_EVERIFY) : NOR_OK;
}
static int erase_sector(struct ota_journal *j, unsigned sector)
{
    uint8_t b[256];
    int rc=j->nor->erase(sector*4096u,4096);
    if (rc) return fail(j,rc);
    for (unsigned pos=0;pos<4096;pos+=256) {
        if ((rc=read_at(j,sector*4096u+pos,b,sizeof b))) return rc;
        for (unsigned i=0;i<sizeof b;i++) if (b[i]!=0xff) return fail(j,NOR_EVERIFY);
    }
    return NOR_OK;
}
/* Commit replacement before erasing old authority; no unknown append tail reuse. */
static int rotate(struct ota_journal *j, const struct ota_progress *p)
{
    uint8_t b[256];
    if (j->sequence==UINT64_MAX) return fail(j,NOR_ERANGE);
    int old=j->sector;
    unsigned next=old<0 ? 0 : ((unsigned)old+1)%SECTORS;
    int rc=erase_sector(j,next);
    if (rc) return rc;
    encode(b,HEADER,j->sequence+1,NULL);
    if ((rc=write_record(j,next*4096u,b))) return rc;
    encode(b,SNAPSHOT,j->sequence+1,p);
    if ((rc=write_record(j,next*4096u+RECORD,b))) return rc;
    j->sequence++; j->progress=*p; j->sector=next; j->next_slot=2; j->fresh=true;
    if (old>=0 && (rc=erase_sector(j,(unsigned)old))) return rc;
    return NOR_OK;
}
static int prepare(struct ota_journal *j)
{
    return !j->fresh || j->next_slot>=16 ? rotate(j,&j->progress) : NOR_OK;
}
static int append(struct ota_journal *j, const struct ota_progress *p)
{
    if (!j->fresh || j->next_slot>=16) return rotate(j,p);
    if (j->sequence==UINT64_MAX) return fail(j,NOR_ERANGE);
    uint8_t b[256]; encode(b,SNAPSHOT,j->sequence+1,p);
    int rc=write_record(j,(unsigned)j->sector*4096u+j->next_slot*RECORD,b);
    if (rc) return rc;
    j->sequence++; j->next_slot++; j->progress=*p;
    return NOR_OK;
}
int ota_journal_recover(struct ota_journal *j, struct nor_port *nor, ota_hash_fn hash, void *ctx)
{
    if (!j || !nor || !nor->read || !nor->program || !nor->erase || !nor->wait_ready ||
        !hash || nor->info.capacity<0x460000u) return NOR_EINVAL;
    memset(j,0,sizeof *j); j->nor=nor; j->hash=hash; j->hash_context=ctx; j->sector=-1;
    int rc=nor->wait_ready(4500000);
    if (rc) return rc;
    uint8_t b[256], best[256]={0};
    for (unsigned s=0;s<SECTORS;s++) {
        if ((rc=read_at(j,s*4096u,b,sizeof b))) return rc;
        if (!valid_record(b,HEADER)) continue;
        uint64_t first=get64(b+8);
        for (unsigned slot=1;slot<16;slot++) {
            struct ota_progress p;
            if ((rc=read_at(j,s*4096u+slot*RECORD,b,sizeof b))) return rc;
            if (!valid_record(b,SNAPSHOT) || get64(b+8)<first || !decode(b,&p)) continue;
            uint64_t seq=get64(b+8);
            if (seq==j->sequence && memcmp(best,b,256)) return NOR_EVERIFY;
            if (seq>j->sequence) {
                j->sequence=seq; j->progress=p; j->sector=s; j->next_slot=slot+1;
                memcpy(best,b,256);
            }
        }
    }
    j->ready=true; j->fresh=false;
    return NOR_OK;
}
int ota_journal_query(const struct ota_journal *j, struct ota_progress *out)
{
    if (!j || !out) return NOR_EINVAL;
    if (!j->ready) return OTA_ERECOVER;
    *out=j->progress; return NOR_OK;
}
int ota_journal_begin(struct ota_journal *j, const struct ota_manifest *m)
{
    if (!j || !m || !manifest_valid(m)) return NOR_EINVAL;
    if (!j->ready) return OTA_ERECOVER;
    if (j->progress.state==OTA_DOWNLOADING || j->progress.state==OTA_VERIFIED) return OTA_EBUSY;
    struct ota_progress p={0}; p.manifest=*m; p.state=OTA_DOWNLOADING;
    return append(j,&p);
}
static int session_check(const struct ota_journal *j, uint64_t id)
{
    if (!j) return NOR_EINVAL;
    if (!j->ready) return OTA_ERECOVER;
    return !id || id!=j->progress.manifest.session_id ? OTA_ESESSION : NOR_OK;
}
int ota_journal_resume(struct ota_journal *j, const struct ota_manifest *m)
{
    if (!m) return NOR_EINVAL;
    int rc=session_check(j,m->session_id);
    if (rc) return rc;
    const struct ota_manifest *old=&j->progress.manifest;
    if (old->target!=m->target || old->length!=m->length || memcmp(old->sha256,m->sha256,32))
        return OTA_ESESSION;
    if (!manifest_valid(m)) return NOR_EINVAL;
    if (j->progress.state!=OTA_DOWNLOADING && j->progress.state!=OTA_VERIFIED) return OTA_ESTATE;
    if (old->source==m->source) return NOR_OK;
    struct ota_progress p=j->progress;p.manifest.source=m->source;
    return append(j,&p);
}
int ota_journal_write_chunk(struct ota_journal *j, uint64_t session_id, uint32_t index,
                             const void *data, size_t length)
{
    int identity=session_check(j,session_id);
    if (identity) return identity;
    if (!data) return NOR_EINVAL;
    if (j->progress.state!=OTA_DOWNLOADING && j->progress.state!=OTA_VERIFIED) return OTA_ESTATE;
    if (index>=chunks(&j->progress)) return NOR_ERANGE;
    uint32_t remaining=j->progress.manifest.length-index*4096u;
    size_t wanted=remaining<4096 ? remaining : 4096;
    if (length!=wanted) return NOR_EINVAL;
    uint32_t address=base(&j->progress)+index*4096u;
    uint8_t b[256]; const uint8_t *p=data;
    int rc;
    if (bit(&j->progress,index)) {
        for (size_t pos=0;pos<length;pos+=sizeof b) {
            size_t n=length-pos<sizeof b ? length-pos : sizeof b;
            if ((rc=read_at(j,address+pos,b,n))) return rc;
            if (memcmp(b,p+pos,n)) return OTA_ECONTENT;
        }
        return NOR_OK;
    }
    if ((rc=prepare(j))) return rc;
    if ((rc=j->nor->erase(address,4096))) return fail(j,rc);
    for (unsigned pos=0;pos<4096;pos+=sizeof b) {
        if ((rc=read_at(j,address+pos,b,sizeof b))) return rc;
        for (unsigned i=0;i<sizeof b;i++) if (b[i]!=0xff) return fail(j,NOR_EVERIFY);
    }
    for (size_t pos=0;pos<length;pos+=8) {
        size_t n=length-pos<8 ? length-pos : 8;
        if ((rc=j->nor->program(address+pos,p+pos,n))) return fail(j,rc);
    }
    for (unsigned pos=0;pos<4096;pos+=sizeof b) {
        if ((rc=read_at(j,address+pos,b,sizeof b))) return rc;
        for (unsigned i=0;i<sizeof b;i++) {
            unsigned k=pos+i;
            if (b[i]!=(k<length ? p[k] : 0xff)) return fail(j,NOR_EVERIFY);
        }
    }
    struct ota_progress next=j->progress;
    next.bitmap[index/8]|=(uint8_t)(1u<<(index%8));
    return append(j,&next);
}
int ota_journal_finish(struct ota_journal *j, uint64_t session_id)
{
    int identity=session_check(j,session_id);
    if (identity) return identity;
    if (j->progress.state==OTA_VERIFIED) return NOR_OK;
    if (j->progress.state!=OTA_DOWNLOADING) return OTA_ESTATE;
    for (unsigned n=0;n<chunks(&j->progress);n++) if (!bit(&j->progress,n)) return OTA_EINCOMPLETE;
    uint8_t digest[32];
    int rc=j->hash(j->nor,base(&j->progress),j->progress.manifest.length,digest,j->hash_context);
    if (rc) return fail(j,rc);
    struct ota_progress p=j->progress;
    bool match=!memcmp(digest,p.manifest.sha256,32);
    p.state=match ? OTA_VERIFIED : OTA_FAILED;
    if ((rc=append(j,&p))) return rc;
    return match ? NOR_OK : OTA_EHASH;
}
int ota_journal_cancel(struct ota_journal *j, uint64_t session_id)
{
    int identity=session_check(j,session_id);
    if (identity) return identity;
    if (j->progress.state==OTA_NONE) return OTA_ESTATE;
    if (j->progress.state==OTA_CANCELLED) return NOR_OK;
    struct ota_progress p=j->progress;
    p.state=OTA_CANCELLED; memset(p.bitmap,0,sizeof p.bitmap);
    return append(j,&p);
}
