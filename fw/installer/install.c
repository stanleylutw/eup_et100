#include "install.h"
#include <string.h>
#include <stdbool.h>
struct verify_context {
    const struct internal_port *port;struct boot_record *record;install_hook hook;
    uint32_t bytes;bool triggered;
};
static int internal_read(void *context,uint32_t off,void *data,size_t len)
{
    struct verify_context *v=context;
    int rc=v->port->read(0x20000+off,data,len);if (rc) return rc;
    v->bytes+=len;
    /* Metadata scan precedes hash; trigger midway through subsequent hash reads. */
    if (!v->triggered && v->bytes>=v->record->length+v->record->length/2) {
        v->triggered=true;if (v->hook) return v->hook(4,v->record);
    }
    return 0;
}
int installer_copy(const struct internal_port *p,image_read_fn source,void *ctx,
                   struct boot_record *record,install_hook hook)
{
    struct image_info info;uint8_t block[256];
    if (record->kind!=BOOT_PENDING) return -1;
    int rc=image_candidate_verify(source,ctx,record->length,record->hash,&info);
    if (rc) return rc;
    uint32_t erase_length=(record->length+4095)&~4095u;
    for (uint32_t off=0;off<erase_length;off+=4096) {
        if (p->erase(0x20000+off,4096)) return -10;
        if (!off && hook && (rc=hook(1,record))) return rc;
    }
    for (uint32_t off=0;off<record->length;) {
        size_t n=record->length-off;if (n>sizeof block) n=sizeof block;
        if (source(ctx,off,block,n) || p->program(0x20000+off,block,n)) return -11;
        if (!off && hook && (rc=hook(2,record))) return rc;
        off+=n;
    }
    struct verify_context verify={.port=p,.record=record,.hook=hook};
    rc=image_candidate_verify(internal_read,&verify,record->length,record->hash,&info);
    if (rc) return rc;
    struct boot_record installed=*record;installed.kind=BOOT_INSTALLED;installed.reset_mask=0;
    if (boot_state_write(p,&installed)) return -12;
    *record=installed;return 0;
}
