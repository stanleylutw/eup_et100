/* TRIAL boot storage probe must not touch other FlashEnv records (auto list). */
#include <assert.h>
#include <stdio.h>
#include "../src/boot_health.c"

static TickType_t ticks;
static unsigned confirms;
static struct { uint32_t key; uint8_t data[128]; size_t size; } db[8];
static unsigned db_count;

TickType_t xTaskGetTickCount(void) { return ticks; }
void flash_erase(void *q,uint32_t o,uint32_t n) { (void)q;(void)o;(void)n;assert(0); }
void flash_write(void *q,uint32_t o,uint32_t n,uint8_t *d) { (void)q;(void)o;(void)n;(void)d;assert(0); }
uint8_t ool_read(uint8_t reg) { (void)reg;return 0; }
void iwdt_Refresh(void) {}
int boot_state_read(const struct internal_port *p,struct boot_record *r)
{ (void)p;memset(r,0,sizeof *r);r->sequence=0x5a5a0007u;r->kind=BOOT_TRIAL;r->length=1024;return 0; }
int boot_state_confirm(const struct internal_port *p,const uint8_t hash[32])
{ (void)p;(void)hash;confirms++;return 0; }
int mcuboot_image_verify(const uint8_t *image,size_t available,struct image_info *info)
{ (void)image;(void)available;memset(info,0,sizeof *info);info->size=1024;return 0; }
int mbedtls_sha256_ret(const unsigned char *in,size_t n,unsigned char out[32],int is224)
{ (void)in;(void)n;(void)is224;memset(out,0,32);return 0; }

static int slot(uint32_t key)
{ for (unsigned i=0;i<db_count;i++) if (db[i].key==key) return (int)i; return -1; }
size_t flashdb_get_length(uint32_t key) { int i=slot(key);return i<0 ? 0 : db[i].size; }
size_t flashdb_get(uint32_t key,uint8_t *data,uint32_t length)
{ int i=slot(key);if(i<0||length<db[i].size)return 0;memcpy(data,db[i].data,db[i].size);return db[i].size; }
int flashdb_set(uint32_t key,uint8_t *data,uint32_t length)
{
    int i=slot(key);if(i<0){assert(db_count<8);i=(int)db_count++;db[i].key=key;}
    assert(length<=sizeof db[i].data);memcpy(db[i].data,data,length);db[i].size=length;return FDB_NO_ERR;
}

int main(void)
{
    const uint32_t keys[]={ET100_FDB_GTIME_KEY,ET100_FDB_CID_KEY,ET100_FDB_AUTO_LIST_KEY,
                           ET100_FDB_SENSOR_PKG_KEY,ET100_FDB_BOOT_HEALTH_KEY};
    for (unsigned i=0;i<5;i++) for (unsigned j=i+1;j<5;j++) assert(keys[i]!=keys[j]);
    /* Keys of records already on flash are frozen. */
    assert(ET100_FDB_GTIME_KEY==0x00030002U && ET100_FDB_CID_KEY==0x00030003U &&
           ET100_FDB_AUTO_LIST_KEY==0x00030004U && ET100_FDB_SENSOR_PKG_KEY==0x00030005U);

    uint8_t auto_list[100];
    for (unsigned i=0;i<sizeof auto_list;i++) auto_list[i]=(uint8_t)(i*7+1);
    flashdb_set(ET100_FDB_AUTO_LIST_KEY,auto_list,sizeof auto_list);
    uint8_t gtime[8]={'E','T',1,0,15,0,0xf0,0xff};
    flashdb_set(ET100_FDB_GTIME_KEY,gtime,sizeof gtime);

    et100_boot_poll(1);
    assert(trial && storage_ok);
    uint32_t marker=0;
    assert(flashdb_get_length(ET100_FDB_BOOT_HEALTH_KEY)==4);
    assert(flashdb_get(ET100_FDB_BOOT_HEALTH_KEY,(uint8_t *)&marker,4)==4 && marker==0x5a5a0007u);
    uint8_t check[100];
    assert(flashdb_get_length(ET100_FDB_AUTO_LIST_KEY)==sizeof auto_list);
    assert(flashdb_get(ET100_FDB_AUTO_LIST_KEY,check,sizeof check)==sizeof check);
    assert(!memcmp(check,auto_list,sizeof auto_list));
    assert(flashdb_get(ET100_FDB_GTIME_KEY,check,8)==8 && !memcmp(check,gtime,8));

    /* Probe passing still gates confirm with ADV + 30 s of healthy loop. */
    et100_boot_adv_started();
    for (ticks=1000;ticks<30000;ticks+=1000) et100_boot_poll(1);
    assert(!confirms);
    ticks=30000;et100_boot_poll(1);
    assert(confirms==1);
    puts("boot health tests passed");
    return 0;
}
