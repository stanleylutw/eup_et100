/* Execute the actual ET-100 SDK adapter against real SDK register types, using
 * RAM registers + fake driver boundaries. No real peripheral is addressed. */
#include "../src/can_task.c"
#include "serial_gchk.h"
#include <assert.h>
#include <stdio.h>
uint32_t fake_can_registers[512];
static unsigned monitor_calls,tx_calls,fifo_count,disabled;
static bool hold_init;
void fake_can_init_stop(struct_CAN_t *r){if(!hold_init)r->CCCtrl.INIT=0;}
static struct_CANRxHeaderDef_t next_header;
static void monitor_check(CAN_HandleTypeDef *h){assert(h->CANx->CCCtrl.MON);assert(!h->CANx->CCCtrl.FDOE);assert(!h->CANx->CCCtrl.BRSE);}
void gpio_init(void *gpio,const GPIO_InitTypeDef *pins){assert(gpio==GPIOA);assert(pins->Pin==(GPIO_PIN_10|GPIO_PIN_11));assert(pins->Alternate==GPIO_FUNCTION_5);}
uint32_t system_get_peripheral_clock(unsigned p){assert(p==PER_CLK_CANx);return 24000000;}
void NVIC_DisableIRQ(unsigned n){assert(n==76);disabled=1;}
void NVIC_EnableIRQ(unsigned n){assert(n==76);assert(disabled);disabled=0;}
void NVIC_SetPriority(unsigned n,unsigned p){assert(n==76&&p==4);}
TickType_t xTaskGetTickCount(void){return 0;}
void vTaskDelay(TickType_t t){(void)t;}
BaseType_t xTaskCreate(void (*f)(void *),const char *n,unsigned s,void *a,unsigned p,void *out){(void)f;(void)n;(void)s;(void)a;(void)out;assert(p==1);return pdPASS;}
size_t flashdb_get_length(uint32_t k){assert(k==ET100_FDB_CAN_KEY);return 0;}
size_t flashdb_get(uint32_t k,uint8_t *b,uint32_t n){(void)k;(void)b;(void)n;return 0;}
int flashdb_set(uint32_t k,uint8_t *b,uint32_t n){(void)k;(void)b;(void)n;return -1;}
uint32_t can_message_ram_init(CAN_HandleTypeDef *h){monitor_check(h);assert(h->RAMConfig.TxFIFOQueueNums==0&&h->RAMConfig.TxDedicatedBufferNums==0);assert(h->RAMConfig.RxFIFO0Nums==64&&h->RAMConfig.ExtendedIDFilterNums==1&&h->RAMConfig.StandardIDFilterNums==0);h->CANx->CCCtrl.INIT=0;h->CANx->CCCtrl.CCE=0;return 2056;}
void can_add_extended_filter(CAN_HandleTypeDef *h,struct_FilterCfg_t f,uint32_t index){monitor_check(h);assert(h->CANx->CCCtrl.INIT&&h->CANx->CCCtrl.CCE);assert(!index&&f.FilterID_1==0&&f.FilterID_2==0x1fffffffu);assert(f.ProcessMode==FILTER_PROCESS_STORE_IN_RxFIFO0);}
void can_int_select_line(CAN_HandleTypeDef *h,uint32_t ints,enum_CAN_INT_LINEx_t line){monitor_check(h);assert(line==CAN_INT_LINE0);assert(ints&INT_RxFIFO0_NEW_MESSAGE);}
void can_int_enable(CAN_HandleTypeDef *h,uint32_t ints){monitor_check(h);(void)ints;}
void can_clear_int_status(CAN_HandleTypeDef *h,uint32_t ints){h->CANx->IntStatus&=~ints;}
uint32_t can_get_int_status(CAN_HandleTypeDef *h){return h->CANx->IntStatus;}
/* Reproduce DAP's stuck INIT state if the redundant SDK helper is called. */
void can_enter_bus_monitoring_mode(CAN_HandleTypeDef *h){monitor_check(h);monitor_calls++;h->CANx->CCCtrl.INIT=1;}
void can_quit_bus_monitoring_mode(CAN_HandleTypeDef *h){(void)h;assert(!"NORMAL forbidden");}
uint8_t can_get_transmit_error_counter(CAN_HandleTypeDef *h){return h->CANx->ErrorCounter.TEC;}
uint8_t can_get_receive_error_counter(CAN_HandleTypeDef *h){return h->CANx->ErrorCounter.REC;}
uint32_t can_get_rxfifo0_fill_level(CAN_HandleTypeDef *h){(void)h;return fifo_count;}
uint32_t can_get_rxfifo0_message(CAN_HandleTypeDef *h,struct_CANRxHeaderDef_t *header,uint8_t *b){monitor_check(h);*header=next_header;memset(b,0,64);assert(fifo_count);fifo_count--;return 0;}
int32_t can_add_tx_message_to_txbuffer(CAN_HandleTypeDef *h,uint32_t i,struct_CANTxHeaderDef_t f,uint8_t *b){(void)h;(void)i;(void)f;(void)b;tx_calls++;assert(!"CAN TX forbidden");return -1;}
static char reply[160];
static void send_line(const char *line,size_t n,void *context){(void)context;assert(n<sizeof reply);memcpy(reply,line,n);reply[n]=0;}
int main(void)
{
    char version_reply[80];
    snprintf(version_reply,sizeof version_reply,"CAN$GVERSION;1;V1:%u;AB12*",ET100_CAN_VERSION);
    et100_sys_init();initialize(1,0);assert(monitor_calls==0&&tx_calls==0);
    assert(!CAN3->CCCtrl.INIT&&CAN3->CCCtrl.MON&&!CAN3->CCCtrl.FDOE&&!CAN3->CCCtrl.BRSE);
    assert(CAN3->NominalTiming.NBRP==3);assert(CAN3->NominalTiming.NTSEG1==19&&CAN3->NominalTiming.NTSEG2==2);
    assert(CAN3->GlobalFilterCfg.ANFS==2&&CAN3->GlobalFilterCfg.ANFE==2);
    assert(CAN3->GlobalFilterCfg.RRFS&&CAN3->GlobalFilterCfg.RRFE);
    assert(et100_can_hw_apply(500));assert(CAN3->NominalTiming.NBRP==1);
    next_header=(struct_CANRxHeaderDef_t){.Identifier=0x18f00400,.DLC=8,.IdType=CAN_ID_STANDARD,.FormatMode=CAN_CLASSICAL_FRAME,.FrameType=CAN_DATA_FRAME};
    fifo_count=1;CAN3->IntStatus=INT_RxFIFO0_NEW_MESSAGE;can3_line0_irq();assert(diagnostics.rejected==1);
    next_header.IdType=CAN_ID_EXTENDED;
    fifo_count=3;CAN3->IntStatus=INT_RxFIFO0_NEW_MESSAGE;can3_line0_irq();et100_can_step(50);
    assert(bitrate.locked&&CAN3->CCCtrl.MON&&tx_calls==0);
    CAN3->ProtocolStatus.Bits.BO=1;CAN3->IntStatus=INT_BUS_OFF_STATUS;can3_line0_irq();et100_can_step(100);
    assert(diagnostics.status==4);et100_can_step(150);assert(diagnostics.status==5);
    CAN3->ProtocolStatus.Bits.BO=0;et100_can_step(200);assert(CAN3->CCCtrl.MON&&diagnostics.status==2&&tx_calls==0);
    struct et100_serial_parser parser;et100_serial_init(&parser,"C03232325903",send_line,NULL);parser.can_command=et100_can_command;
    const char *query="CAN$GVERSION;AB12\r\n";
    for(const char *p=query;*p;p++)et100_serial_feed(&parser,(uint8_t)*p);
    et100_can_step(250);bool found=false;while(et100_can_take_line(reply,sizeof reply,false))if(strstr(reply,version_reply))found=true;
    assert(found&&tx_calls==0);
    /* DAP failure fixture: MON alone must not publish a working controller. */
    hold_init=true;initialize(1,0);
    assert(CAN3->CCCtrl.INIT&&CAN3->CCCtrl.MON);
    assert(diagnostics.status==6&&!bitrate.running&&disabled&&monitor_calls==0);
    hold_init=false;et100_can_step(500);
    assert(diagnostics.status==2&&bitrate.running&&!CAN3->CCCtrl.INIT&&!disabled&&tx_calls==0);
    puts("PASS CAN SDK adapter: MON always, classic-only/filter/RAM, exact timing, ISR/SYS recovery, zero TX and serial CAN$ dispatch");
}
