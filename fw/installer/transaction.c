#include "transaction.h"
#include "inst_status.h"
#include <string.h>

struct region { struct nor_port *nor;uint32_t base; };
int transaction_prepare(const struct internal_port *p,struct boot_record *s,struct inst_record *r,int nor_rc)
{
    if (nor_rc) {
        r->stage=IS_NOR;r->code=IC_NOR_INIT;
        if (s->kind!=BOOT_PENDING) return nor_rc;
        s->kind=BOOT_CONFIRMED;s->last_update=UPDATE_ABORTED;s->reset_mask=0;
        if (boot_state_write(p,s)) { r->stage=IS_STATE_WRITE;r->code=IC_STATE_WRITE;return -12; }
        return 0;
    }
    bool same_hash=!memcmp(r->hash,s->hash,32);
    unsigned reason=r->result==IR_TIMEOUT && same_hash ? IC_TRIAL_TIMEOUT : IC_TRIAL_RESET;
    if (same_hash && r->state==BOOT_ROLLBACK_REQUIRED && r->result==IR_RUNNING &&
        r->code>=IC_TRIAL_TIMEOUT && r->code<=IC_RETRY_LIMIT) reason=r->code;
    enum inst_retry action=inst_attempt(r,s);
    if (s->kind==BOOT_ROLLBACK_REQUIRED) r->code=reason;
    if (action==INST_CONTINUE) return 0;
    s->kind=action==INST_ABORT ? BOOT_CONFIRMED : BOOT_ROLLBACK_REQUIRED;
    if (action==INST_ABORT) s->last_update=UPDATE_ABORTED;
    s->reset_mask=0;
    if (boot_state_write(p,s)) { r->stage=IS_STATE_WRITE;r->code=IC_STATE_WRITE;return -12; }
    r->code=IC_RETRY_LIMIT;r->state=s->kind;r->attempts=1;
    return 0;
}
static int external(void *context,uint32_t off,void *data,size_t n)
{
    struct region *r=context;
    if (off>0x200000 || n>0x200000-off) return -1;
    int rc=r->nor->read(r->base+off,data,n);
#if defined(ET100_TEST_BAD_CANDIDATE) && ET100_TEST_BAD_CANDIDATE
    /* Test-only read corruption: never alter physical NOR or the Rollback source. */
    if (!rc && r->base==0x20000 && off<=520 && n>520-off) ((uint8_t *)data)[520-off]^=1;
#endif
    return rc;
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
    if (rc) { inst_report(IS_BACKUP,0,s->confirmed_length,IC_BACKUP_FAIL);return rc; }
    /* A matching string in state is insufficient: read and verify actual NOR bytes. */
    if (!image_candidate_verify(external,&rollback,s->confirmed_length,s->confirmed_hash,&info)) {
        inst_report(IS_BACKUP,UINT32_MAX,s->confirmed_length,0);return 0;
    }
    inst_report(IS_BACKUP,0,s->confirmed_length,0);
    uint8_t buffer[256];uint32_t length=(s->confirmed_length+4095)&~4095u;
    for (uint32_t off=0;off<length;off+=4096) {
        if ((rc=nor->erase(0x220000+off,4096))) { inst_report(IS_BACKUP,off,s->confirmed_length,IC_BACKUP_FAIL);return rc; }
        inst_feed();
        for (uint32_t at=off;at<off+4096 && at<s->confirmed_length;) {
            size_t n=s->confirmed_length-at;if (n>sizeof buffer) n=sizeof buffer;
            if ((rc=internal((void *)p,at,buffer,n)) || (rc=nor->program(0x220000+at,buffer,n))) {
                inst_report(IS_BACKUP,at,s->confirmed_length,IC_BACKUP_FAIL);return rc;
            }
            inst_feed();
            at+=n;
        }
        if (!off && hook && (rc=hook(8,s))) return rc;
        uint32_t done=off+4096;if (done>s->confirmed_length) done=s->confirmed_length;
        inst_report(IS_BACKUP,done,s->confirmed_length,0);
    }
    rc=image_candidate_verify(external,&rollback,s->confirmed_length,s->confirmed_hash,&info);
    if (rc) inst_report(IS_BACKUP,0,s->confirmed_length,IC_BACKUP_FAIL);
    return rc;
}
int transaction_run(const struct internal_port *p,struct nor_port *nor,
                    struct boot_record *s,install_hook hook)
{
    struct image_info info;struct region candidate={nor,0x20000},rollback={nor,0x220000};int rc;
    if (s->kind==BOOT_TRIAL) {
        s->kind=BOOT_ROLLBACK_REQUIRED;
        if (boot_state_write(p,s)) { inst_report(IS_STATE_WRITE,0,0,IC_STATE_WRITE);return -12; }
    }
    if (s->kind==BOOT_PENDING) {
        rc=image_candidate_verify(external,&candidate,s->length,s->hash,&info);
        inst_report(IS_CHECK,rc ? 0 : 1,1,rc ? IC_SRC_INVALID : IC_OK);
        if (!rc) rc=backup(p,nor,s,hook);
        /* Test hooks model reset, not an I/O error. Preserve PENDING for reboot. */
        if (rc==-99) return rc;
        if (rc) {
            s->kind=BOOT_CONFIRMED;s->last_update=UPDATE_ABORTED;s->reset_mask=0;
            if (boot_state_write(p,s)) { inst_report(IS_STATE_WRITE,0,0,IC_STATE_WRITE);return -12; }
            return 0;
        }
        s->kind=BOOT_BACKUP_READY;
        if (boot_state_write(p,s)) { inst_report(IS_STATE_WRITE,0,0,IC_STATE_WRITE);return -12; }
        inst_report(IS_STATE_WRITE,s->kind,0,0);
    }
    if (s->kind==BOOT_BACKUP_READY) return installer_copy(p,external,&candidate,s,hook);
    if (s->kind==BOOT_ROLLBACK_REQUIRED) return installer_copy(p,external,&rollback,s,hook);
    return s->kind==BOOT_CONFIRMED ? 0 : -1;
}
