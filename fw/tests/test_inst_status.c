#include "inst_status.h"
#include "fake_nor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct fake_nor baseline,device;
int main(void)
{
    assert(inst_crc16("123456789",9)==0x31c3);
    uint8_t hash[32]={0x87,0x55,0xce,0x49};char line[121];
    assert(inst_line(line,sizeof line,'L',"WRITE",hash,"262144;314752"));
    assert(!strcmp(line,"ET100$INST;1;L;WRITE;8755CE49;262144;314752*2926\r\n"));
    assert(inst_line(line,sizeof line,'L',"HELLO",hash,"0.8.11;PENDING"));
    assert(strstr(line,"*8208\r\n"));
    assert(!inst_line(line,10,'L',"WRITE",hash,"1;2"));
    assert(!inst_line(line,sizeof line,'L',"WRITE",hash,"bad*crc"));
    assert(!inst_line(line,sizeof line,'Z',"WRITE",hash,""));
    assert(!inst_line(line,sizeof line,'L',"write",hash,""));
    char big[130];memset(big,'X',129);big[129]=0;
    assert(!inst_line(line,sizeof line,'L',"WRITE",hash,big));
    struct inst_progress progress={0};unsigned lines=0;size_t bytes=0;
    for(unsigned stage=IS_BACKUP;stage<=IS_WRITE;stage++) {
        for(uint32_t done=0;done<=0x1c0000;done+=256) if(inst_progress_due(&progress,stage,done,0x1c0000)) {
            char fields[32];snprintf(fields,sizeof fields,"%u;%u",done,0x1c0000);
            size_t n=inst_line(line,sizeof line,'L',inst_stage_name(stage),hash,fields);
            assert(n&&n<=120);bytes+=n;lines++;
        }
    }
    assert(lines==33 && bytes+15*INST_LINE_MAX<=6144);
    progress=(struct inst_progress){0};assert(inst_progress_due(&progress,IS_WRITE,0,4096));
    assert(!inst_progress_due(&progress,IS_WRITE,0,4096));
    struct boot_record boot={.kind=BOOT_PENDING};memcpy(boot.hash,hash,32);
    struct inst_record record={0};
    for(unsigned i=1;i<=3;i++) { assert(inst_attempt(&record,&boot)==INST_CONTINUE);assert(record.attempts==i); }
    assert(inst_attempt(&record,&boot)==INST_ABORT);
    boot.kind=BOOT_BACKUP_READY;
    for(unsigned i=1;i<=3;i++) { assert(inst_attempt(&record,&boot)==INST_CONTINUE);assert(record.attempts==i); }
    assert(inst_attempt(&record,&boot)==INST_ROLLBACK);
    boot.kind=BOOT_ROLLBACK_REQUIRED;
    for(unsigned i=0;i<100;i++) assert(inst_attempt(&record,&boot)==INST_CONTINUE);
    boot.kind=BOOT_PENDING;boot.hash[0]^=1;
    assert(inst_attempt(&record,&boot)==INST_CONTINUE && record.attempts==1);
    record.result=IR_ABORTED;assert(inst_attempt(&record,&boot)==INST_CONTINUE && record.attempts==1);
    for(unsigned i=0;i<2;i++)assert(inst_attempt(&record,&boot)==INST_CONTINUE);
    boot.hash[31]^=1;assert(inst_attempt(&record,&boot)==INST_CONTINUE&&record.attempts==1);

    fake_nor_create(&baseline);fake_nor_create(&device);fake_nor_select(&baseline);
    memset(baseline.bytes,0xa5,INST_RECORD_BASE);
    assert(!inst_record_read(&baseline.port,&record));
    (void)inst_attempt(&record,&boot);record.result=IR_INSTALLED;
    assert(!inst_record_write(&baseline.port,&record));
    struct inst_record old;assert(inst_record_read(&baseline.port,&old));
    record.result=IR_ABORTED;record.code=IC_SRC_INVALID;
    fake_nor_copy(&device,&baseline);fake_nor_select(&device);
    assert(!inst_record_write(&device.port,&record));uint64_t operations=device.operations;
    unsigned cuts=0;
    for(unsigned mode=0;mode<2;mode++) for(uint64_t op=1;op<=operations;op++) {
        fake_nor_copy(&device,&baseline);fake_nor_select(&device);jmp_buf loss;
        fake_nor_inject(&device,op,mode,&loss);
        if (!setjmp(loss)) (void)inst_record_write(&device.port,&record);
        device.cut_at=0;
        struct inst_record recovered;assert(inst_record_read(&device.port,&recovered));
        assert(recovered.result==IR_INSTALLED || recovered.result==IR_ABORTED);
        assert(!memcmp(device.bytes,baseline.bytes,INST_RECORD_BASE)); /* Journal/candidate/sensor untouched. */
        assert(!inst_record_write(&device.port,&record));
        assert(inst_record_read(&device.port,&recovered) && recovered.result==IR_ABORTED);
        cuts++;
    }
    memset(device.bytes+INST_RECORD_BASE,0,8192);device.cut_at=0;
    assert(!inst_record_read(&device.port,&record) && record.attempts==0);
    fake_nor_destroy(&device);fake_nor_destroy(&baseline);
    printf("PASS INST CRC vectors, limits, retry policy, corrupt-as-zero, dual-sector power cuts %u\n",cuts);
    return 0;
}
