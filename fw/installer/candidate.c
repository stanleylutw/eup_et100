#include "mcuboot_image.h"
#include "eup_fw_info.h"
#include "integrity.h"
#include "mbedtls/sha256.h"
#include <string.h>

static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int image_hash(image_read_fn read, void *ctx, uint32_t length, uint8_t digest[32])
{
    uint8_t block[256]; int rc=0;mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);rc=mbedtls_sha256_starts_ret(&sha,0);
    for (uint32_t off=0;!rc && off<length;) {
        size_t n=length-off;if (n>sizeof block) n=sizeof block;
        if (read(ctx,off,block,n)) { rc=IMAGE_EIO;break; }
        rc=mbedtls_sha256_update_ret(&sha,block,n);off+=n;
    }
    if (!rc) rc=mbedtls_sha256_finish_ret(&sha,digest);
    mbedtls_sha256_free(&sha);return rc;
}
static int metadata(image_read_fn read,void *ctx,uint32_t length)
{
    uint8_t block[263], raw[286];unsigned found=0;
    for (uint32_t off=0;off<length;off+=256) {
        size_t n=length-off;if (n>sizeof block) n=sizeof block;
        if (read(ctx,off,block,n)) return IMAGE_EIO;
        for (unsigned i=0;i<256 && i+8<=n;i++) {
            /* Split literal: this file is also linked into the App, whose image must
             * contain exactly one contiguous metadata magic (the real block). */
            if (block[i]!='E' || memcmp(block+i+1,"UPFWV1",7)) continue;
            if (++found>1 || length-off-i<sizeof raw) return IMAGE_EMETADATA;
            if (read(ctx,off+i,raw,sizeof raw)) return IMAGE_EIO;
            if (raw[8]!=1 || raw[9]) return IMAGE_EMETADATA;
            if (memcmp(raw+10,"EUP_ET100",9) || memcmp(raw+34,"ET100_EVB_",10))
                return IMAGE_ETARGET;
            if (!memchr(raw+10,0,24) || !memchr(raw+34,0,20) || !memchr(raw+54,0,32))
                return IMAGE_EMETADATA;
            uint32_t crc=get32(raw+282);
            /* FR3068 requires a provided CRC; nRF's zero exception is App-side only. */
            if (!crc || crc!=installer_crc32(raw,282)) return IMAGE_EMETADATA;
        }
    }
    return found==1 ? IMAGE_OK : IMAGE_EMETADATA;
}
int image_candidate_verify(image_read_fn read,void *ctx,uint32_t length,
                           const uint8_t expected[32],struct image_info *info)
{
    uint8_t header[32],vector[8],tlv[40],digest[32];
    memset(info,0,sizeof *info);
    if (length<560 || length>ET100_APP_SLOT_SIZE) return IMAGE_EFORMAT;
    if (read(ctx,0,header,sizeof header)) return IMAGE_EIO;
    /* Check target first so genuine nRF packages produce an explicit target rejection. */
    int rc=metadata(read,ctx,length);if (rc) return rc;
    uint32_t payload=get32(header+12);
    if (get32(header)!=0x96f3b83d || get32(header+4) || header[8]!=0 || header[9]!=2 ||
        header[10] || header[11] || get32(header+16) || payload<8 || payload!=length-552)
        return IMAGE_EFORMAT;
    uint32_t pos=512+payload;
    if (read(ctx,pos,tlv,40) || read(ctx,512,vector,8)) return IMAGE_EIO;
    if (memcmp(tlv,"\x07\x69\x28\x00\x10\x00\x20\x00",8)) return IMAGE_EFORMAT;
    rc=image_hash(read,ctx,pos,digest);if (rc) return rc;
    if (memcmp(digest,tlv+8,32)) return IMAGE_EHASH;
    memcpy(info->sha256,digest,32);
    rc=image_hash(read,ctx,length,digest);if (rc) return rc;
    if (memcmp(digest,expected,32)) return IMAGE_EHASH;
    uint32_t sp=get32(vector),pc=get32(vector+4);
    if (sp<=0x20000000 || sp>0x20080000 || (sp&7) || !(pc&1) ||
        (pc&~1u)<ET100_APP_VECTOR || (pc&~1u)>=ET100_APP_VECTOR+payload) return IMAGE_EVECTOR;
    info->size=length;info->payload_size=payload;info->msp=sp;info->reset=pc;
    return 0;
}
