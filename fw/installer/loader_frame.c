#include "loader_frame.h"
#include "integrity.h"
#include <string.h>
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int loader_header(const uint8_t h[20],uint8_t *op,uint32_t *index,size_t *length)
{
    if (memcmp(h,"LDR1",4) || h[5] || installer_crc32(h,16)!=get32(h+16)) return -1;
    *op=h[4];*index=get32(h+8);*length=h[6]|(unsigned)h[7]<<8;
    if (*op<LOAD_BEGIN || *op>LOAD_ABORT || *length>LOADER_MAX ||
        (*op==LOAD_BEGIN && *length!=48) || (*op==LOAD_CHUNK && !*length) ||
        (*op>=LOAD_FINISH && *length)) return -1;
    return 0;
}
int loader_payload(const uint8_t h[20],const uint8_t *payload)
{ return installer_crc32(payload,h[6]|(unsigned)h[7]<<8)==get32(h+12) ? 0 : -1; }
