#include "fr30xx.h"
#include "mcuboot_image.h"
#include "mbedtls/sha256.h"
#include "device.h"
#include "install.h"
#include "transaction.h"
#include "loader_frame.h"
#include "ota_journal.h"
#include "nor_qspi1.h"
#include <string.h>
#include <stdio.h>

static UART_HandleTypeDef uart;
extern void installer_jump(const struct image_info *) __attribute__((noreturn));
int _write(int fd, char *data, int length)
{
    (void)fd;uart_transmit(&uart,(uint8_t *)data,length);return length;
}
bool user_deep_sleep_check(void) { return false; }
void user_entry_before_sleep(void) {}
void user_entry_after_sleep(void) {}
void user_entry_after_sleep_user(void) {}
static void board_init(void)
{
    pmu_init();
    iwdt_Disable(); /* M-WDT proved this stops the inherited PMU watchdog. */
    System_ClkConfig_t c={0};c.SPLL_CFG.PLL_N=8;c.SPLL_CFG.PowerEn=1;
    c.MCU_Clock_Source=MCU_CLK_SEL_SPLL_CLK;c.SOC_DIV=1;c.MCU_DIV=2;
    c.APB0_DIV=c.APB1_DIV=c.APB2_DIV=1;
    System_SPLL_config(&c.SPLL_CFG,1000);System_MCU_clock_Config(&c);
    __SYSTEM_UART_CLK_SELECT_SPLL();
    GPIO_InitTypeDef g={0};g.Pin=GPIO_PIN_4|GPIO_PIN_5;g.Mode=GPIO_MODE_AF_PP;
    g.Pull=GPIO_PULLUP;g.Alternate=GPIO_FUNCTION_1;gpio_init(GPIOB,&g);
    __SYSTEM_UART3_CLK_ENABLE();uart.UARTx=UART3;uart.Init.BaudRate=115200;
    uart.Init.DataLength=UART_DATA_LENGTH_8BIT;uart.Init.StopBits=UART_STOPBITS_1;
    uart.Init.Parity=UART_PARITY_NONE;uart.Init.FIFO_Mode=UART_FIFO_ENABLE;
    uart_init(&uart);setvbuf(stdout,NULL,_IONBF,0);
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
}
static uint8_t payload[4096];
static struct ota_journal journal;
static uint32_t loader_now(void)
{
    /* pyOCD clears DEMCR.TRCENA on disconnect; keep the IRQ-free clock alive. */
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    return nor_qspi1_now_us();
}
static int receive(void *data,size_t length,uint32_t timeout_us)
{
    uint8_t *p=data;uint32_t start=loader_now();uint64_t budget=(uint64_t)timeout_us*96+1;
    while (length && budget--) {
        if (uart.UARTx->LSR.LSR_BIT.DR) { uart_receive(&uart,p++,1);length--; }
        if ((uint32_t)(loader_now()-start)>=timeout_us) break;
    }
    return length ? -1 : 0;
}
static unsigned loader_requested(void)
{
    char line[16];unsigned used=0;
    uint32_t start=loader_now();uint64_t budget=192000001;
    printf("LOADER WINDOW 2000ms token=ET100-LOAD\r\n");
    while ((uint32_t)(loader_now()-start)<2000000 && budget--) {
        if (!uart.UARTx->LSR.LSR_BIT.DR) continue;
        uint8_t ch;uart_receive(&uart,&ch,1);
        if (ch=='\r') continue;
        if (ch=='\n') {
            line[used]=0;
            if (!strcmp(line,"ET100-LOAD")) return 1;
            if (!strcmp(line,"ET100-WDT")) return 2;
            used=0;
        } else if (used<sizeof line-1) line[used++]=ch;
        else used=0;
    }
    return false;
}
static void watchdog_start(unsigned seconds)
{
    iwdt_Init_t cfg={.iwdt_Count=32000u*seconds,.iwdt_Timeout=1024,.iwdt_int_Enable=WDT_INT_DISABLE};
    iwdt_init(cfg);iwdt_Enable();iwdt_Refresh();
}
static void watchdog_probe(void)
{
    printf("WDT PROBE start 2s feed for 4s\r\n");watchdog_start(2);
    uint32_t start=loader_now(),last=start;
    while ((uint32_t)(loader_now()-start)<4000000) {
        if ((uint32_t)(loader_now()-last)>=250000) { iwdt_Refresh();last=loader_now(); }
    }
    printf("WDT FEED PASS ctrl=%02X\r\n",ool_read(PMU_REG_WDT_CTRL));
    iwdt_Disable();printf("WDT DISABLE requested ctrl=%02X wait 4s\r\n",ool_read(PMU_REG_WDT_CTRL));
    start=loader_now();while ((uint32_t)(loader_now()-start)<4000000) {}
    printf("WDT DISABLE PASS; restart 2s NO FEED expect reset\r\n");watchdog_start(2);
    start=loader_now();while ((uint32_t)(loader_now()-start)<6000000) {}
    printf("WDT RESET FAILED STOP\r\n");for (;;) __WFI();
}
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static int dev_loader(void)
{
    int rc=installer_nor_init();struct nor_port *nor=nor_qspi1_port();
    if (!rc) rc=nor_qspi1_authorize_writes();
    if (!rc) rc=ota_journal_recover(&journal,nor,installer_journal_hash,NULL);
    if (rc) return rc;
    printf("LOADER READY dev-only UNSIGNED\r\n");
    bool session=false;uint64_t sid=0;uint32_t reset_mask=0;
    for (;;) {
        uint8_t h[20],op;uint32_t index;size_t n;
        if (receive(h,sizeof h,30000000) || loader_header(h,&op,&index,&n)) return -30;
        if (receive(payload,n,3000000) || loader_payload(h,payload)) return -31;
        rc=0;
        if (op==LOAD_ABORT) { printf("ACK 4 0 rc=0 ABORT\r\n");return 0; }
        if (op==LOAD_BEGIN) {
            struct ota_manifest m={0};
            m.session_id=(uint64_t)get32(payload)|(uint64_t)get32(payload+4)<<32;
            m.length=get32(payload+8);m.target=OTA_ET100;m.source=OTA_RS232;
            memcpy(m.sha256,payload+12,32);reset_mask=get32(payload+44);
            if (reset_mask&~15u || m.length>ET100_APP_SLOT_SIZE || m.length<560) rc=-32;
            else {
                /* Explicit new begin discards the previous dev session, never boot state. */
                if (journal.progress.state==OTA_DOWNLOADING || journal.progress.state==OTA_VERIFIED ||
                    journal.progress.state==OTA_FAILED)
                    rc=ota_journal_cancel(&journal,journal.progress.manifest.session_id);
                if (!rc) rc=ota_journal_begin(&journal,&m);
                if (!rc) { sid=m.session_id;session=true; }
            }
        } else if (!session) rc=OTA_ESESSION;
        else if (op==LOAD_CHUNK) rc=ota_journal_write_chunk(&journal,sid,index,payload,n);
        else if (op==LOAD_FINISH) {
            rc=ota_journal_finish(&journal,sid);
            struct image_info info;
            if (!rc) rc=image_candidate_verify(installer_candidate_read,NULL,
                journal.progress.manifest.length,journal.progress.manifest.sha256,&info);
            if (!rc) {
                struct boot_record state;
                rc=boot_state_read(&installer_internal,&state);
                if (!rc && state.kind!=BOOT_CONFIRMED) rc=-33;
                if (!rc) {
                    state.kind=BOOT_PENDING;state.length=journal.progress.manifest.length;
                    state.reset_mask=reset_mask;memcpy(state.hash,journal.progress.manifest.sha256,32);
                    rc=boot_state_write(&installer_internal,&state);
                }
            }
        }
        printf("ACK %u %lu rc=%d\r\n",op,(unsigned long)index,rc);
        if (op==LOAD_FINISH && !rc) { nor_qspi1_delay_us(20000);NVIC_SystemReset(); }
    }
}
static int reset_hook(unsigned phase,struct boot_record *state)
{
    if (!(state->reset_mask&phase)) return 0;
    state->reset_mask&=~phase;
    if (boot_state_write(&installer_internal,state)) return -40;
    printf("INST TEST_RESET phase=%u remaining=%lu state retained\r\n",phase,(unsigned long)state->reset_mask);
    nor_qspi1_delay_us(20000);NVIC_SystemReset();return -41;
}
int main(void)
{
    board_init();printf("ET100 INSTALLER 0.8.4-dev UNSIGNED PROTOTYPE\r\n");
    const uint8_t *b=(const uint8_t *)0x08001000;
    bool zero=true, ff=true;
    for (unsigned i=0;i<4096;i++) { zero&=b[i]==0;ff&=b[i]==0xff; }
    uint8_t digest[32];mbedtls_sha256_ret(b,4096,digest,0);
    printf("BINFO kind=%s sha256=",zero ? "ZERO" : ff ? "FF" : "MIXED-WARNING");
    for (unsigned i=0;i<32;i++) printf("%02X",digest[i]);
    printf("\r\n");
    struct boot_record state;
    int rc=boot_state_read(&installer_internal,&state);
    printf("STATE rc=%d kind=%lu seq=%lu len=%lu reset_mask=%lu\r\n",rc,
        (unsigned long)state.kind,(unsigned long)state.sequence,(unsigned long)state.length,(unsigned long)state.reset_mask);
    if (rc) goto stop;
    if (state.kind==BOOT_TRIAL) {
        state.kind=BOOT_ROLLBACK_REQUIRED;
        if ((rc=boot_state_write(&installer_internal,&state))) goto stop;
        printf("TRIAL unconfirmed: ROLLBACK_REQUIRED\r\n");
    }
    if (state.kind==BOOT_PENDING || state.kind==BOOT_BACKUP_READY || state.kind==BOOT_ROLLBACK_REQUIRED) {
        rc=installer_nor_init();
        if (rc && state.kind==BOOT_PENDING) {
            state.kind=BOOT_CONFIRMED;state.last_update=UPDATE_ABORTED;
            rc=boot_state_write(&installer_internal,&state);
        } else if (!rc) {
            if (state.kind==BOOT_PENDING) rc=nor_qspi1_authorize_writes();
            if (!rc) rc=transaction_run(&installer_internal,nor_qspi1_port(),&state,reset_hook);
        }
        printf("INST COMPLETE rc=%d kind=%lu\r\n",rc,(unsigned long)state.kind);
        if (rc) goto stop;
    } else {
        unsigned request=loader_requested();
        if (request==2) watchdog_probe();
        if (request==1) {
            rc=dev_loader();printf("LOADER EXIT rc=%d\r\n",rc);
            if (rc) goto stop;
        }
    }
    struct image_info info;
    rc=mcuboot_image_verify((const uint8_t *)ET100_APP_SLOT,ET100_APP_SLOT_SIZE,&info);
    if (!rc && state.kind==BOOT_CONFIRMED) {
        mbedtls_sha256_ret((const uint8_t *)ET100_APP_SLOT,info.size,digest,0);
        if (info.size!=state.confirmed_length || memcmp(digest,state.confirmed_hash,32)) rc=IMAGE_EHASH;
    }
    printf("E0 VERIFY rc=%d vector=0x%08lX payload=%lu\r\n",rc,
           (unsigned long)ET100_APP_VECTOR,(unsigned long)info.payload_size);
    if (!rc) {
        printf("BOOT kind=%lu last_update=%lu\r\n",(unsigned long)state.kind,(unsigned long)state.last_update);
        watchdog_start(60);
        printf("%s WDT 60s enabled\r\n",state.kind==BOOT_TRIAL ? "TRIAL" : "BOOT");
        printf("E0 JUMP msp=0x%08lX reset=0x%08lX irq=masked\r\n",
               (unsigned long)info.msp,(unsigned long)info.reset);
        for (unsigned i=0;i<1000000;i++) if (uart.UARTx->LSR.LSR_BIT.TEMT) installer_jump(&info);
    }
stop:
    printf("INST STOP rc=%d; fail closed\r\n",rc);
    for (;;) __WFI();
}
