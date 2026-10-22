#include "device.h"
#include "fr30xx.h"
#include "nor_qspi1.h"
#include "mcuboot_image.h"
#include <string.h>

static int read_internal(uint32_t off,void *data,size_t n)
{
    if (off>0x200000 || n>0x200000-off) return -1;
    memcpy(data,(const void *)(0x08000000u+off),n);return 0;
}
static bool writable(uint32_t off,size_t n)
{
    return (off>=0x20000 && off<0x1e0000 && n<=0x1e0000-off) ||
           (off>=BOOT_STATE_BASE && off<0x200000 && n<=0x200000-off);
}
static int erase_internal(uint32_t off,size_t n)
{
    if (!n || (off&4095) || (n&4095) || !writable(off,n)) return -1;
    flash_erase(QSPI0,off,n);
    const volatile uint8_t *p=(const volatile uint8_t *)(0x08000000u+off);
    for (size_t i=0;i<n;i++) if (p[i]!=0xff) return -1;
    return 0;
}
static int program_internal(uint32_t off,const void *data,size_t n)
{
    if (!n || !writable(off,n) || n>256) return -1;
    /* Source must be SRAM: SDK disables XIP while the RAM flash routine runs. */
    uint8_t buffer[256];memcpy(buffer,data,n);flash_write(QSPI0,off,n,buffer);
    return memcmp((const void *)(0x08000000u+off),data,n) ? -1 : 0;
}
const struct internal_port installer_internal={read_internal,erase_internal,program_internal};
int installer_nor_init(void)
{
    struct nor_port *p=nor_qspi1_port();int rc=p->init();
    /* Initialization is read-only; authorizing writes is confined to the dev loader. */
    return rc;
}
int installer_candidate_read(void *ctx,uint32_t off,void *data,size_t n)
{
    (void)ctx;if (off>0x200000 || n>0x200000-off) return -1;
    return nor_qspi1_port()->read(0x20000+off,data,n);
}
struct hash_region { struct nor_port *port;uint32_t base; };
static int region_read(void *context,uint32_t off,void *data,size_t n)
{
    struct hash_region *r=context;return r->port->read(r->base+off,data,n);
}
int installer_journal_hash(struct nor_port *p,uint32_t off,uint32_t n,uint8_t digest[32],void *ctx)
{
    (void)ctx;struct hash_region r={p,off};return image_hash(region_read,&r,n,digest);
}
