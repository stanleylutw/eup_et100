#include "mcuboot_image.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put32(uint8_t *p,uint32_t v) { for (unsigned i=0;i<4;i++) p[i]=v>>(8*i); }
static void rehash(uint8_t *p,size_t n) { assert(!mbedtls_sha256_ret(p,n-40,p+n-32,0)); }
int main(int argc,char **argv)
{
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long size=ftell(f);assert(size>0);
    rewind(f);uint8_t *b=malloc(size),*original=malloc(size);assert(b && original);
    assert(fread(b,1,size,f)==(size_t)size);fclose(f);memcpy(original,b,size);
    struct image_info info;
    assert(mcuboot_image_verify(b,size,&info)==IMAGE_OK && info.size==(uint32_t)size);
    for (size_t n=0;n<(size_t)size;n++) assert(mcuboot_image_verify(b,n,&info)!=IMAGE_OK);
    const unsigned offsets[]={0,4,8,10,12,16};
    for (unsigned i=0;i<sizeof offsets/sizeof offsets[0];i++) {
        memcpy(b,original,size);b[offsets[i]]^=1;
        assert(mcuboot_image_verify(b,size,&info)!=IMAGE_OK);
    }
    memcpy(b,original,size);b[0x240]^=1;assert(mcuboot_image_verify(b,size,&info)==IMAGE_EHASH);
    memcpy(b,original,size);b[size-1]^=1;assert(mcuboot_image_verify(b,size,&info)==IMAGE_EHASH);
    memcpy(b,original,size);b[size-40]^=1;assert(mcuboot_image_verify(b,size,&info)==IMAGE_EFORMAT);
    memcpy(b,original,size);put32(b+12,UINT32_MAX);assert(mcuboot_image_verify(b,size,&info)==IMAGE_EFORMAT);
    const uint32_t bad_sp[]={0x20000000,0x20080008,0x20001001,0x08021000};
    for (unsigned i=0;i<4;i++) {
        memcpy(b,original,size);put32(b+0x200,bad_sp[i]);rehash(b,size);
        assert(mcuboot_image_verify(b,size,&info)==IMAGE_EVECTOR);
    }
    const uint32_t bad_pc[]={0x08020200,0x08002001,0x081e0001,0x20001001};
    for (unsigned i=0;i<4;i++) {
        memcpy(b,original,size);put32(b+0x204,bad_pc[i]);rehash(b,size);
        assert(mcuboot_image_verify(b,size,&info)==IMAGE_EVECTOR);
    }
    free(b);free(original);puts("PASS MCUboot unsigned profile: official imgtool fixture, all truncations, flags/length/TLV/hash/vector rejection");
}
