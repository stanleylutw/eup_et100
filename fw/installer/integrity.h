#ifndef ET100_INTEGRITY_H
#define ET100_INTEGRITY_H
#include <stddef.h>
#include <stdint.h>
static inline uint32_t installer_crc32(const void *data, size_t length)
{
    const uint8_t *p=data; uint32_t crc=~0u;
    while (length--) {
        crc^=*p++;
        for (unsigned i=0;i<8;i++) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));
    }
    return ~crc;
}
#endif
