#include "boot_health.h"
#include "boot_state.h"
#include "mcuboot_image.h"
#include "mbedtls/sha256.h"
#include "fr30xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fdb_app.h"
#include <string.h>

static volatile unsigned advertising_started;
static TickType_t started,last_feed,last_poll;
static bool initialized,trial,storage_ok;
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
static bool confirmed;
#endif
static uint8_t own_hash[32];
static int read_flash(uint32_t off,void *data,size_t n)
{
    if (off>0x200000 || n>0x200000-off) return -1;
    memcpy(data,(const void *)(0x08000000u+off),n);return 0;
}
static int erase_state(uint32_t off,size_t n)
{
    if (off<BOOT_STATE_BASE || off>=0x200000 || n>0x200000-off || (off&4095) || (n&4095)) return -1;
    flash_erase(QSPI0,off,n);
    const volatile uint8_t *p=(const volatile uint8_t *)(0x08000000u+off);
    for (size_t i=0;i<n;i++) if (p[i]!=0xff) return -1;
    return 0;
}
static int program_state(uint32_t off,const void *data,size_t n)
{
    if (off<BOOT_STATE_BASE || off>=0x200000 || n>0x200000-off || n>256) return -1;
    uint8_t copy[256];memcpy(copy,data,n);flash_write(QSPI0,off,n,copy);
    return memcmp((const void *)(0x08000000u+off),data,n) ? -1 : 0;
}
static const struct internal_port port={read_flash,erase_state,program_state};
void et100_boot_adv_started(void) { advertising_started=1; }
void et100_boot_poll(int storage_ready)
{
    TickType_t now=xTaskGetTickCount();
    if (initialized && (TickType_t)(now-last_poll)>pdMS_TO_TICKS(2000)) started=now;
    last_poll=now;
    if (!initialized) {
        initialized=true;started=now;last_feed=now;
        struct boot_record state;struct image_info info;
        if (!boot_state_read(&port,&state) && state.kind==BOOT_TRIAL &&
            !mcuboot_image_verify((const uint8_t *)ET100_APP_SLOT,ET100_APP_SLOT_SIZE,&info) &&
            !mbedtls_sha256_ret((const uint8_t *)ET100_APP_SLOT,info.size,own_hash,0) &&
            info.size==state.length && !memcmp(own_hash,state.hash,32)) {
            trial=true;
            if (storage_ready) {
                uint32_t marker=state.sequence,verify=0;
                storage_ok=flashdb_set(0x00030004u,(uint8_t *)&marker,4)==FDB_NO_ERR &&
                    flashdb_get(0x00030004u,(uint8_t *)&verify,4)==4 && verify==marker;
            }
        }
    }
#if defined(ET100_TEST_HANG) && ET100_TEST_HANG
    if (trial && (TickType_t)(now-started)>=pdMS_TO_TICKS(5000)) {
        __disable_irq();for (;;) __NOP(); /* Deliberate test: no other task can feed. */
    }
#endif
    if ((TickType_t)(now-last_feed)>=pdMS_TO_TICKS(1000)) {
        if (ool_read(PMU_REG_WDT_CTRL)&PMU_WDT_EN_BIT) iwdt_Refresh();
        last_feed=now;
    }
#if !defined(ET100_TEST_NO_CONFIRM) || !ET100_TEST_NO_CONFIRM
    if (trial && !confirmed && storage_ready && storage_ok && advertising_started &&
        (TickType_t)(now-started)>=pdMS_TO_TICKS(30000)) {
        /* Serialize both-bank transaction against scheduling and flash users. */
        taskENTER_CRITICAL();confirmed=boot_state_confirm(&port,own_hash)==0;taskEXIT_CRITICAL();
    }
#endif
}
