#include "inst_status.h"
#include "../installer/integrity.h"
#include <stdio.h>
#include <string.h>

static inst_report_fn reporter;
static void (*feed_fn)(void);
bool inst_progress_due(struct inst_progress *p,unsigned stage,uint32_t done,uint32_t total)
{
    if (!total || done>total) return false;
    unsigned bucket=(unsigned)((uint64_t)done*10/total);
    if (p->seen && p->stage==stage && p->bucket==bucket) return false;
    p->seen=true;p->stage=stage;p->bucket=bucket;return true;
}
void inst_set_reporter(inst_report_fn fn) { reporter=fn; }
void inst_report(enum inst_stage s,uint32_t done,uint32_t total,unsigned code)
{ if (reporter) reporter(s,done,total,code); }
void inst_set_feed(void (*fn)(void)) { feed_fn=fn; }
void inst_feed(void) { if (feed_fn) feed_fn(); }
uint16_t inst_crc16(const void *data,size_t n)
{
    const uint8_t *p=data;uint16_t crc=0;
    while (n--) { crc^=(uint16_t)*p++<<8;
        for (unsigned i=0;i<8;i++) crc=(uint16_t)((crc&0x8000) ? (crc<<1)^0x1021 : crc<<1); }
    return crc;
}
void inst_txn(const uint8_t hash[32],char out[9])
{
    if (!hash) { strcpy(out,"-");return; }
    for (unsigned i=0;i<4;i++) snprintf(out+i*2,3,"%02X",hash[i]);
}
size_t inst_line(char *out,size_t cap,char src,const char *event,const uint8_t hash[32],const char *fields)
{
    char txn[9];inst_txn(hash,txn);
    if (!out || !event || !*event || !fields || (src!='L' && src!='A')) return 0;
    for (const char *p=event;*p;p++) if (*p<'A' || *p>'Z') {
        if (*p!='_') return 0;
    }
    for (const char *p=fields;*p;p++) if (*p<0x20 || *p>0x7e || *p=='*') return 0;
    int n=snprintf(out,cap,"ET100$INST;1;%c;%s;%s%s%s",src,event,txn,*fields?";":"",fields);
    if (n<0 || (size_t)n+7>INST_LINE_MAX || (size_t)n+8>cap) return 0;
    uint16_t crc=inst_crc16(out,(size_t)n);
    snprintf(out+n,cap-(size_t)n,"*%04X\r\n",crc);
    return (size_t)n+7;
}
const char *inst_state_name(uint32_t s)
{
    static const char *const names[]={"NONE","PENDING","CONFIRMED","BACKUP_READY","TRIAL","ROLLBACK_REQUIRED"};
    return s<6 ? names[s] : "CORRUPT";
}
const char *inst_result_name(uint32_t r)
{
    static const char *const names[]={"-","RUNNING","INSTALLED","ABORTED","ROLLED_BACK","TRIAL_TIMEOUT","HALT","CONFIRMED"};
    return r<8 ? names[r] : "-";
}
const char *inst_stage_name(uint32_t s)
{
    static const char *const names[]={"NONE","NOR_INIT","STATE_READ","STATE_WRITE","CHECK","BACKUP","ERASE","WRITE","VERIFY","BOOT"};
    return s<10 ? names[s] : "NONE";
}
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(uint8_t *p,uint32_t n) { for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(n>>(i*8)); }
/* EIR1: 128-byte LE wire record. Commit is separately programmed last. */
static bool decode(const uint8_t b[128],struct inst_record *r)
{
    if (memcmp(b,"EIR1",4) || get32(b+4)!=1 || get32(b+120)!=0x54494d43 ||
        get32(b+112)!=installer_crc32(b,112)) return false;
    r->sequence=get32(b+8);memcpy(r->hash,b+12,32);
    r->state=get32(b+44);r->attempts=get32(b+48);r->result=get32(b+52);
    r->code=get32(b+56);r->stage=get32(b+60);
    return r->state<=BOOT_ROLLBACK_REQUIRED && r->result<=IR_CONFIRMED &&
        r->code<=IC_RESULT_UNAVAILABLE && r->stage<=IS_BOOT;
}
static int scan(struct nor_port *p,struct inst_record out[2])
{
    uint8_t b[128];bool valid[2]={false,false};
    if (!p || p->info.capacity<INST_RECORD_END) return -1;
    for (unsigned i=0;i<2;i++)
        if (!p->read(INST_RECORD_BASE+i*4096,b,sizeof b)) valid[i]=decode(b,&out[i]);
    if (valid[0] && valid[1]) {
        if (out[0].sequence==out[1].sequence) return -1;
        return (int32_t)(out[1].sequence-out[0].sequence)>0 ? 1 : 0;
    }
    return valid[1] ? 1 : valid[0] ? 0 : -1;
}
int inst_record_read(struct nor_port *p,struct inst_record *out)
{
    struct inst_record r[2];memset(out,0,sizeof *out);int bank=scan(p,r);
    if (bank<0) return 0;
    *out=r[bank];return 1;
}
int inst_record_write(struct nor_port *p,const struct inst_record *r)
{
    if (!p || p->info.capacity<INST_RECORD_END) return -1;
    struct inst_record old[2],check;int bank=scan(p,old);
    uint32_t off=INST_RECORD_BASE+(bank==0 ? 4096 : 0);
    uint8_t b[128]={0},verify[128];
    memcpy(b,"EIR1",4);put32(b+4,1);put32(b+8,bank<0 ? 1 : old[bank].sequence+1);
    memcpy(b+12,r->hash,32);put32(b+44,r->state);put32(b+48,r->attempts);
    put32(b+52,r->result);put32(b+56,r->code);put32(b+60,r->stage);
    memcpy(b+64,INST_VERSION,sizeof INST_VERSION);
    put32(b+112,installer_crc32(b,112));put32(b+120,0x54494d43);
    if (!decode(b,&check) || p->erase(off,4096)) return -1;
    inst_feed();
    for (unsigned i=0;i<120;i+=8) if (p->program(off+i,b+i,8)) return -1;
    if (p->read(off,verify,120) || memcmp(b,verify,120) ||
        p->program(off+120,b+120,8) || p->read(off,verify,128) || memcmp(b,verify,128)) return -1;
    inst_feed();return 0;
}
enum inst_retry inst_attempt(struct inst_record *r,const struct boot_record *s)
{
    bool same=!memcmp(r->hash,s->hash,32) && r->state==s->kind && r->result==IR_RUNNING;
    uint32_t count=same ? r->attempts : 0;
    enum inst_retry action=INST_CONTINUE;
    if (count>=3 && s->kind==BOOT_PENDING) action=INST_ABORT;
    if (count>=3 && s->kind==BOOT_BACKUP_READY) action=INST_ROLLBACK;
    memcpy(r->hash,s->hash,32);r->state=s->kind;
    r->attempts=count==UINT32_MAX ? count : count+1;
    r->result=IR_RUNNING;r->code=IC_OK;r->stage=IS_NONE;
    return action;
}
