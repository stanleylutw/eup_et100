#include "fake_nor.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static struct fake_nor *device;
static bool interrupt_now(void) { return ++device->operations==device->cut_at; }
static void power_loss(void)
{
    device->cuts++; device->cut_at=0;
    assert(device->power_loss);
    longjmp(*device->power_loss,1);
}
static int wait_ready(uint32_t us)
{
    assert(us>0); if (interrupt_now()) power_loss(); return NOR_OK;
}
static int read_data(uint32_t a, void *p, size_t n)
{
    assert(a<=FAKE_NOR_SIZE && n<=FAKE_NOR_SIZE-a);
    device->reads++;
    if (interrupt_now()) power_loss();
    memcpy(p,device->bytes+a,n);
    /* A weak erased sector is not reliable even if its visible cells are FF. */
    for (size_t i=0;i<n;i++) if (device->weak[(a+i)/4096]) {
        ((uint8_t *)p)[i]^=(uint8_t)(1u<<((device->reads+i)%8)); break;
    }
    return NOR_OK;
}
static int program(uint32_t a, const void *data, size_t n)
{
    const uint8_t *p=data;
    assert(n && n<=8 && a<FAKE_NOR_SIZE && n<=FAKE_NOR_SIZE-a);
    assert(a/256==(a+n-1)/256 && a/8==(a+n-1)/8);
    assert(!device->weak[a/4096]);
    assert(!device->used[a/8]); /* Even an all-FF torn PP consumes this location. */
    for (size_t i=0;i<n;i++) assert((device->bytes[a+i]&p[i])==p[i]);
    device->programs++;device->used[a/8]=1;
    if (interrupt_now()) {
        for (size_t i=0;i<n;i++) {
            if (device->mode==0 && i<n/2) device->bytes[a+i]&=p[i];
            if (device->mode==1) device->bytes[a+i]&=(uint8_t)(p[i]|0xaa);
        }
        power_loss();
    }
    for (size_t i=0;i<n;i++) device->bytes[a+i]&=p[i];
    return NOR_OK;
}
static int erase(uint32_t a, uint32_t n)
{
    assert(n==4096 && a%4096==0 && a<FAKE_NOR_SIZE && n<=FAKE_NOR_SIZE-a);
    device->erases++;
    if (interrupt_now()) {
        if (device->mode==0) {
            for (unsigned i=0;i<n;i++) if ((i*1103515245u+12345u)&0x80000000u) device->bytes[a+i]=0xff;
        } else memset(device->bytes+a,0xff,n);
        device->weak[a/4096]=true;
        power_loss();
    }
    memset(device->bytes+a,0xff,n);memset(device->used+a/8,0,n/8);
    device->weak[a/4096]=false;return NOR_OK;
}
void fake_nor_select(struct fake_nor *f) { device=f; }
void fake_nor_clear(struct fake_nor *f)
{
    memset(f->bytes,0xff,FAKE_NOR_SIZE);memset(f->used,0,FAKE_NOR_SIZE/8);
    memset(f->weak,0,sizeof f->weak);
    f->operations=f->programs=f->erases=f->reads=f->cut_at=f->cuts=0;
    f->power_loss=NULL;
}
void fake_nor_create(struct fake_nor *f)
{
    memset(f,0,sizeof *f);
    f->bytes=malloc(FAKE_NOR_SIZE);f->used=malloc(FAKE_NOR_SIZE/8);
    assert(f->bytes && f->used);fake_nor_clear(f);
    f->port.info.capacity=FAKE_NOR_SIZE;f->port.info.page_size=256;
    f->port.read=read_data;f->port.program=program;f->port.erase=erase;f->port.wait_ready=wait_ready;
}
void fake_nor_destroy(struct fake_nor *f) { free(f->bytes);free(f->used); }
void fake_nor_copy(struct fake_nor *to, const struct fake_nor *from)
{
    memcpy(to->bytes,from->bytes,FAKE_NOR_SIZE);memcpy(to->used,from->used,FAKE_NOR_SIZE/8);
    memcpy(to->weak,from->weak,sizeof to->weak);
    to->operations=to->programs=to->erases=to->reads=to->cut_at=to->cuts=0;
    to->power_loss=NULL;
}
void fake_nor_inject(struct fake_nor *f, uint64_t operation, unsigned mode, jmp_buf *env)
{
    f->cut_at=operation;f->mode=mode;f->power_loss=env;
}
