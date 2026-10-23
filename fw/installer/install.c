#include "install.h"
#include "inst_status.h"
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
    if (record->kind!=BOOT_BACKUP_READY && record->kind!=BOOT_ROLLBACK_REQUIRED) return -1;
    struct boot_record image=*record;
    if (record->kind==BOOT_ROLLBACK_REQUIRED) {
        image.length=record->confirmed_length;memcpy(image.hash,record->confirmed_hash,32);
    }
    int rc=image_candidate_verify(source,ctx,image.length,image.hash,&info);
    inst_report(IS_CHECK,rc ? 0 : 1,1,rc ? IC_SRC_INVALID : IC_OK);
    if (rc) return rc;
    uint32_t erase_length=(image.length+4095)&~4095u;
    inst_report(IS_ERASE,0,erase_length,0);
    for (uint32_t off=0;off<erase_length;off+=4096) {
        if (p->erase(0x20000+off,4096)) { inst_report(IS_ERASE,off,erase_length,IC_ERASE_FAIL);return -10; }
        inst_feed();inst_report(IS_ERASE,off+4096,erase_length,0);
        if (!off && hook && (rc=hook(1,record))) return rc;
    }
    inst_report(IS_WRITE,0,image.length,0);
    for (uint32_t off=0;off<image.length;) {
        size_t n=image.length-off;if (n>sizeof block) n=sizeof block;
        if (source(ctx,off,block,n) || p->program(0x20000+off,block,n)) {
            inst_report(IS_WRITE,off,image.length,IC_WRITE_FAIL);return -11;
        }
        if (!off && hook && (rc=hook(2,record))) return rc;
        off+=n;
        inst_feed();inst_report(IS_WRITE,off,image.length,0);
    }
    struct verify_context verify={.port=p,.record=&image,.hook=hook};
    rc=image_candidate_verify(internal_read,&verify,image.length,image.hash,&info);
    inst_report(IS_VERIFY,rc ? 0 : 1,1,rc ? IC_VERIFY_FAIL : IC_OK);
    if (rc) return rc;
    struct boot_record installed=*record;
    installed.kind=record->kind==BOOT_ROLLBACK_REQUIRED ? BOOT_CONFIRMED : BOOT_TRIAL;
    if (record->kind==BOOT_ROLLBACK_REQUIRED) installed.last_update=UPDATE_FAILED;
    installed.reset_mask=0;
    if (boot_state_write(p,&installed)) { inst_report(IS_STATE_WRITE,0,0,IC_STATE_WRITE);return -12; }
    *record=installed;return 0;
}
