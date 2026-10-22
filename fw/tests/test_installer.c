#include "install.h"
#include "loader_frame.h"
#include "integrity.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t flash[0x200000],saved[0x200000],original[0x200000];
static uint8_t *image;static size_t image_length;
static unsigned calls,cut,cut_mode;
static unsigned hook_expected;
static int read_flash(uint32_t off,void *data,size_t n)
{ assert(off<=sizeof flash && n<=sizeof flash-off);memcpy(data,flash+off,n);return 0; }
static bool allowed(uint32_t off,size_t n)
{ return (off>=0x20000 && off<0x1e0000 && n<=0x1e0000-off) ||
         (off>=0x1fc000 && off<0x200000 && n<=0x200000-off); }
static int erase_flash(uint32_t off,size_t n)
{
    assert(allowed(off,n) && !(off&4095) && !(n&4095));bool fail=++calls==cut;
    size_t done=fail ? (cut_mode==0 ? 0 : cut_mode==1 ? n/2 : n) : n;
    memset(flash+off,0xff,done);return fail ? -1 : 0;
}
static int program_flash(uint32_t off,const void *data,size_t n)
{
    assert(allowed(off,n));const uint8_t *p=data;bool fail=++calls==cut;
    size_t done=fail ? (cut_mode==0 ? 0 : cut_mode==1 ? n/2 : n) : n;
    for (size_t i=0;i<done;i++) { assert((flash[off+i]&p[i])==p[i]);flash[off+i]&=p[i]; }
    return fail ? -1 : 0;
}
static const struct internal_port port={read_flash,erase_flash,program_flash};
static int reset_hook(unsigned phase,struct boot_record *record)
{
    if (!(record->reset_mask&phase)) return 0;
    assert(phase==hook_expected);hook_expected<<=1;record->reset_mask&=~phase;
    assert(!boot_state_write(&port,record));return -99;
}
static int source(void *ctx,uint32_t off,void *data,size_t n)
{ (void)ctx;if (off>image_length || n>image_length-off) return -1;memcpy(data,image+off,n);return 0; }
static void put32(uint8_t *p,uint32_t n) { for (unsigned i=0;i<4;i++) p[i]=n>>(8*i); }
static void rehash(void)
{ assert(!mbedtls_sha256_ret(image,image_length-40,image+image_length-32,0)); }
static void digest(uint8_t out[32]) { assert(!mbedtls_sha256_ret(image,image_length,out,0)); }
static void protected_check(void)
{
    assert(!memcmp(flash,original,0x20000));
    assert(!memcmp(flash+0x1e0000,original+0x1e0000,0x1c000));
}
int main(int argc,char **argv)
{
    assert(!allowed(0x1000,4096) && !allowed(0x1e0000,4096) && !allowed(UINT32_MAX,1));
    assert(argc==3);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);image_length=ftell(f);
    rewind(f);image=malloc(image_length);assert(image && fread(image,1,image_length,f)==image_length);fclose(f);
    uint8_t *backup=malloc(image_length);assert(backup);memcpy(backup,image,image_length);
    uint8_t hash[32];struct image_info info;digest(hash);
    assert(!image_candidate_verify(source,NULL,image_length,hash,&info));
    size_t meta=0;while (meta+286<=image_length && memcmp(image+meta,"EUPFWV1\0",8)) meta++;
    assert(meta+286<=image_length);assert(installer_crc32("123456789",9)==0xcbf43926);
    assert(installer_crc32(image+meta,282)==(uint32_t)(image[meta+282]|(uint32_t)image[meta+283]<<8|
        (uint32_t)image[meta+284]<<16|(uint32_t)image[meta+285]<<24));
    put32(image+meta+282,0);rehash();digest(hash);
    assert(image_candidate_verify(source,NULL,image_length,hash,&info)==IMAGE_EMETADATA);
    put32(image+meta+282,1);rehash();digest(hash);
    assert(image_candidate_verify(source,NULL,image_length,hash,&info)==IMAGE_EMETADATA);
    memcpy(image,backup,image_length);image[meta+10]='X';rehash();digest(hash);
    assert(image_candidate_verify(source,NULL,image_length,hash,&info)==IMAGE_ETARGET);
    memcpy(image,backup,image_length);image[meta+34]='X';rehash();digest(hash);
    assert(image_candidate_verify(source,NULL,image_length,hash,&info)==IMAGE_ETARGET);
    memcpy(image,backup,image_length);image[0x240]^=1;digest(hash);
    assert(image_candidate_verify(source,NULL,image_length,hash,&info)==IMAGE_EHASH);
    memcpy(image,backup,image_length);digest(hash);hash[0]^=1;
    assert(image_candidate_verify(source,NULL,image_length,hash,&info)==IMAGE_EHASH);digest(hash);

    uint8_t frame[20]={ 'L','D','R','1',LOAD_CHUNK,0,4,0 };put32(frame+12,installer_crc32("test",4));
    put32(frame+16,installer_crc32(frame,16));uint8_t op;uint32_t index;size_t n;
    assert(!loader_header(frame,&op,&index,&n) && n==4 && op==LOAD_CHUNK);
    assert(!loader_payload(frame,(const uint8_t *)"test"));
    assert(loader_payload(frame,(const uint8_t *)"fail"));
    for (unsigned i=0;i<20;i++) { frame[i]^=1;assert(loader_header(frame,&op,&index,&n));frame[i]^=1; }
    frame[6]=1;frame[7]=16;put32(frame+16,installer_crc32(frame,16));assert(loader_header(frame,&op,&index,&n));

    memset(flash,0xa5,sizeof flash);memset(flash+0x1fc000,0xff,0x4000);memcpy(original,flash,sizeof flash);
    struct boot_record pending={.kind=BOOT_BACKUP_READY,.length=image_length,.confirmed_length=image_length};
    memcpy(pending.hash,hash,32);memcpy(pending.confirmed_hash,hash,32);
    assert(!boot_state_write(&port,&pending));memcpy(saved,flash,sizeof flash);calls=0;
    assert(!installer_copy(&port,source,NULL,&pending,NULL));unsigned operations=calls;
    struct boot_record record;assert(!boot_state_read(&port,&record) && record.kind==BOOT_TRIAL);
    assert(!memcmp(flash+0x20000,image,image_length));protected_check();
    unsigned injections=0;
    for (unsigned mode=0;mode<3;mode++) for (unsigned at=1;at<=operations;at++) {
        memcpy(flash,saved,sizeof flash);assert(!boot_state_read(&port,&record));
        calls=0;cut=at;cut_mode=mode;assert(installer_copy(&port,source,NULL,&record,NULL));
        cut=0;assert(!boot_state_read(&port,&record));
        assert(record.kind==BOOT_BACKUP_READY || record.kind==BOOT_TRIAL);
        if (record.kind==BOOT_BACKUP_READY) assert(!installer_copy(&port,source,NULL,&record,NULL));
        assert(!boot_state_read(&port,&record) && record.kind==BOOT_TRIAL);
        assert(!memcmp(flash+0x20000,image,image_length));protected_check();injections++;
    }
    /* Wrong hash/target must not erase even one App or state byte. */
    memcpy(flash,saved,sizeof flash);assert(!boot_state_read(&port,&record));calls=0;
    image[0x240]^=1;assert(installer_copy(&port,source,NULL,&record,NULL));
    assert(!calls && !memcmp(flash,saved,sizeof flash));image[0x240]^=1;
    memcpy(flash,saved,sizeof flash);assert(!boot_state_read(&port,&record));record.reset_mask=7;
    assert(!boot_state_write(&port,&record));hook_expected=1;
    for (unsigned i=0;i<3;i++) {
        assert(!boot_state_read(&port,&record) && record.kind==BOOT_BACKUP_READY);
        assert(installer_copy(&port,source,NULL,&record,reset_hook)!=0);
    }
    assert(hook_expected==8);assert(!boot_state_read(&port,&record) && !record.reset_mask);
    assert(!installer_copy(&port,source,NULL,&record,reset_hook));protected_check();
    /* With no trustworthy record, never infer that an uncommitted install succeeded. */
    memset(flash+0x1fc000,0,0x4000);assert(boot_state_read(&port,&record));
    f=fopen(argv[2],"rb");assert(f);fseek(f,0,SEEK_END);size_t nrf_length=ftell(f);rewind(f);
    free(image);image=malloc(nrf_length);image_length=nrf_length;assert(image && fread(image,1,nrf_length,f)==nrf_length);fclose(f);
    digest(hash);assert(image_candidate_verify(source,NULL,image_length,hash,&info)==IMAGE_ETARGET);
    puts("PASS FR metadata CRC zero/mismatch rejection and valid nonzero acceptance, real nRF target rejection, loader framing");
    printf("PASS install power-cut matrix %u injections (%u operations x3), pending recovery, protected ranges\n",injections,operations);
    free(image);free(backup);return 0;
}
