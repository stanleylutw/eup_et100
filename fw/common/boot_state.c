#include "boot_state.h"
#include "../installer/integrity.h"
#include <stdbool.h>
#include <string.h>

#define STATE_MAGIC 0x32545342u
#define STATE_COMMIT 0x54494d43u
struct disk_record {
    uint32_t magic;
    struct boot_record value;
    uint32_t crc,commit;
};
_Static_assert(sizeof(struct disk_record)==100,"boot state v2 wire size");
static bool valid(const struct disk_record *r)
{
    return r->magic==STATE_MAGIC && r->commit==STATE_COMMIT &&
        r->crc==installer_crc32(r,offsetof(struct disk_record,crc)) &&
        r->value.kind<=BOOT_ROLLBACK_REQUIRED && !(r->value.reset_mask&~15u) &&
        r->value.last_update<=UPDATE_ABORTED &&
        (r->value.kind==BOOT_NONE || (r->value.length>=560 && r->value.length<=0x1c0000)) &&
        (r->value.kind==BOOT_NONE || r->value.kind==BOOT_PENDING ||
         (r->value.confirmed_length>=560 && r->value.confirmed_length<=0x1c0000));
}
static bool decode(struct disk_record *r)
{
    if (valid(r)) return true;
    uint8_t raw[60];memcpy(raw,r,sizeof raw);uint32_t crc,commit;
    memcpy(&crc,raw+52,4);memcpy(&commit,raw+56,4);
    uint32_t magic;memcpy(&magic,raw,4);
    if (magic!=0x31545342 || commit!=STATE_COMMIT || crc!=installer_crc32(raw,52)) return false;
    struct boot_record legacy={0};memcpy(&legacy,raw+4,48);
    if (legacy.kind>BOOT_INSTALLED || (legacy.reset_mask&~7u) ||
        (legacy.kind && (legacy.length<560 || legacy.length>0x1c0000))) return false;
    if (legacy.kind==BOOT_INSTALLED) {
        legacy.confirmed_length=legacy.length;memcpy(legacy.confirmed_hash,legacy.hash,32);
    }
    memset(r,0,sizeof *r);r->magic=STATE_MAGIC;r->value=legacy;
    r->crc=installer_crc32(r,offsetof(struct disk_record,crc));r->commit=STATE_COMMIT;
    return true;
}
static int scan(const struct internal_port *p,struct disk_record r[2])
{
    for (unsigned i=0;i<2;i++) if (p->read(BOOT_STATE_BASE+i*BOOT_STATE_BANK,&r[i],sizeof r[i])) return -2;
    bool a=decode(&r[0]),b=decode(&r[1]);
    if (a && b) {
        if (r[0].value.sequence==r[1].value.sequence && memcmp(r,r+1,sizeof *r)) return -2;
        return (int32_t)(r[1].value.sequence-r[0].value.sequence)>0 ? 1 : 0;
    }
    if (a || b) return b ? 1 : 0;
    return -1;
}
int boot_state_read(const struct internal_port *p,struct boot_record *out)
{
    struct disk_record r[2];int bank=scan(p,r);memset(out,0,sizeof *out);
    if (bank>=0) { *out=r[bank].value;return 0; }
    if (bank==-2) return -1;
    uint8_t data[128];
    for (unsigned pos=0;pos<2*BOOT_STATE_BANK;pos+=sizeof data) {
        if (p->read(BOOT_STATE_BASE+pos,data,sizeof data)) return -1;
        for (unsigned i=0;i<sizeof data;i++) if (data[i]!=0xff) return -1;
    }
    return 0;
}
int boot_state_confirm(const struct internal_port *p,const uint8_t hash[32])
{
    struct boot_record state;
    if (boot_state_read(p,&state) || state.kind!=BOOT_TRIAL || memcmp(state.hash,hash,32)) return -1;
    state.kind=BOOT_CONFIRMED;state.confirmed_length=state.length;
    memcpy(state.confirmed_hash,hash,32);state.last_update=UPDATE_OK;
    return boot_state_write(p,&state);
}
int boot_state_write(const struct internal_port *p,const struct boot_record *record)
{
    struct disk_record old[2],next,check;struct boot_record current;
    if (boot_state_read(p,&current)) return -1;
    int bank=scan(p,old);if (bank==-2) return -1;
    uint32_t off=BOOT_STATE_BASE+(bank==0 ? BOOT_STATE_BANK : 0);
    memset(&next,0,sizeof next);next.magic=STATE_MAGIC;next.value=*record;
    next.value.sequence=current.sequence+1;
    next.crc=installer_crc32(&next,offsetof(struct disk_record,crc));next.commit=STATE_COMMIT;
    if (!valid(&next) || p->erase(off,BOOT_STATE_BANK)) return -1;
    uint8_t erased[128];
    for (unsigned i=0;i<BOOT_STATE_BANK;i+=sizeof erased) {
        if (p->read(off+i,erased,sizeof erased)) return -1;
        for (unsigned k=0;k<sizeof erased;k++) if (erased[k]!=0xff) return -1;
    }
    if (p->program(off,&next,offsetof(struct disk_record,commit)) ||
        p->read(off,&check,offsetof(struct disk_record,commit)) ||
        memcmp(&next,&check,offsetof(struct disk_record,commit))) return -1;
    if (p->program(off+offsetof(struct disk_record,commit),&next.commit,4) ||
        p->read(off,&check,sizeof check) || memcmp(&check,&next,sizeof check) || !valid(&check)) return -1;
    return 0;
}
