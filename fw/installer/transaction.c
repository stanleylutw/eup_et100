#include "transaction.h"
#include <string.h>

struct region { struct nor_port *nor;uint32_t base; };
static int external(void *context,uint32_t off,void *data,size_t n)
{
    struct region *r=context;
    if (off>0x200000 || n>0x200000-off) return -1;
    return r->nor->read(r->base+off,data,n);
}
static int internal(void *context,uint32_t off,void *data,size_t n)
{
    const struct internal_port *p=context;
    if (off>0x1c0000 || n>0x1c0000-off) return -1;
    return p->read(0x20000+off,data,n);
}
static int backup(const struct internal_port *p,struct nor_port *nor,
                  struct boot_record *s,install_hook hook)
{
    struct image_info info;struct region rollback={nor,0x220000};
    int rc=image_candidate_verify(internal,(void *)p,s->confirmed_length,s->confirmed_hash,&info);
    if (rc) return rc;
    /* A matching string in state is insufficient: read and verify actual NOR bytes. */
    if (!image_candidate_verify(external,&rollback,s->confirmed_length,s->confirmed_hash,&info)) return 0;
    uint8_t buffer[256];uint32_t length=(s->confirmed_length+4095)&~4095u;
    for (uint32_t off=0;off<length;off+=4096) {
        if ((rc=nor->erase(0x220000+off,4096))) return rc;
        for (uint32_t at=off;at<off+4096 && at<s->confirmed_length;) {
            size_t n=s->confirmed_length-at;if (n>sizeof buffer) n=sizeof buffer;
            if ((rc=internal((void *)p,at,buffer,n)) || (rc=nor->program(0x220000+at,buffer,n))) return rc;
            at+=n;
        }
        if (!off && hook && (rc=hook(8,s))) return rc;
    }
    return image_candidate_verify(external,&rollback,s->confirmed_length,s->confirmed_hash,&info);
}
int transaction_run(const struct internal_port *p,struct nor_port *nor,
                    struct boot_record *s,install_hook hook)
{
    struct image_info info;struct region candidate={nor,0x20000},rollback={nor,0x220000};int rc;
    if (s->kind==BOOT_TRIAL) {
        s->kind=BOOT_ROLLBACK_REQUIRED;
        if (boot_state_write(p,s)) return -12;
    }
    if (s->kind==BOOT_PENDING) {
        rc=image_candidate_verify(external,&candidate,s->length,s->hash,&info);
        if (!rc) rc=backup(p,nor,s,hook);
        /* Test hooks model reset, not an I/O error. Preserve PENDING for reboot. */
        if (rc==-99) return rc;
        if (rc) {
            s->kind=BOOT_CONFIRMED;s->last_update=UPDATE_ABORTED;s->reset_mask=0;
            if (boot_state_write(p,s)) return -12;
            return 0;
        }
        s->kind=BOOT_BACKUP_READY;
        if (boot_state_write(p,s)) return -12;
    }
    if (s->kind==BOOT_BACKUP_READY) return installer_copy(p,external,&candidate,s,hook);
    if (s->kind==BOOT_ROLLBACK_REQUIRED) return installer_copy(p,external,&rollback,s,hook);
    return s->kind==BOOT_CONFIRMED ? 0 : -1;
}
