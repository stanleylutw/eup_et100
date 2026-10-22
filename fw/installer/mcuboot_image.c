#include "mcuboot_image.h"
#include "mbedtls/sha256.h"
#include <string.h>

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
int mcuboot_image_verify(const uint8_t *b, size_t n, struct image_info *out)
{
    if (out) memset(out,0,sizeof *out);
    if (!b || !out || n<0x200+8+40 || n>ET100_APP_SLOT_SIZE) return IMAGE_EFORMAT;
    if (le32(b)!=0x96f3b83d || le32(b+4)!=0 || le16(b+8)!=0x200 ||
        le16(b+10)!=0 || le32(b+16)!=0) return IMAGE_EFORMAT;
    uint32_t payload=le32(b+12);
    if (payload<8 || payload>n-0x200-40) return IMAGE_EFORMAT;
    size_t tlv=0x200+payload;
    /* This milestone accepts exactly the official unsigned SHA-256 profile. */
    if (le16(b+tlv)!=0x6907 || le16(b+tlv+2)!=40 ||
        b[tlv+4]!=0x10 || b[tlv+5]!=0 || le16(b+tlv+6)!=32) return IMAGE_EFORMAT;
    uint8_t digest[32];
    if (mbedtls_sha256_ret(b,tlv,digest,0) || memcmp(digest,b+tlv+8,32)) return IMAGE_EHASH;
    uint32_t msp=le32(b+0x200), reset=le32(b+0x204);
    if (msp<=0x20000000 || msp>0x20080000 || (msp&7) || !(reset&1) ||
        (reset&~1u)<ET100_APP_VECTOR || (reset&~1u)>=ET100_APP_VECTOR+payload)
        return IMAGE_EVECTOR;
    out->size=tlv+40;out->payload_size=payload;out->msp=msp;out->reset=reset;
    memcpy(out->sha256,digest,32);return IMAGE_OK;
}
