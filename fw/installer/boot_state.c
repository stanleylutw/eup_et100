#include "boot_state.h"
#include "integrity.h"
#include <stdbool.h>
#include <string.h>

#define STATE_MAGIC 0x31545342u
#define STATE_COMMIT 0x54494d43u
struct disk_record {
    uint32_t magic;
    struct boot_record value;
    uint32_t crc,commit;
};
_Static_assert(sizeof(struct disk_record)==60,"boot state wire size");
static bool valid(const struct disk_record *r)
{
    return r->magic==STATE_MAGIC && r->commit==STATE_COMMIT &&
        r->crc==installer_crc32(r,offsetof(struct disk_record,crc)) &&
        r->value.kind<=BOOT_INSTALLED && !(r->value.reset_mask&~7u) &&
        (r->value.kind==BOOT_NONE || (r->value.length>=560 && r->value.length<=0x1c0000));
}
static int scan(const struct internal_port *p,struct disk_record r[2])
{
    for (unsigned i=0;i<2;i++) if (p->read(BOOT_STATE_BASE+i*BOOT_STATE_BANK,&r[i],sizeof r[i])) return -2;
    bool a=valid(&r[0]),b=valid(&r[1]);
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
