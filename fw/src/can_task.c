/* ET-100 EVB CAN3 owner. Portable F407 modules remain unmodified in src/can/.
 * Hardware is always classic CAN bus monitoring: never ACK, transmit or request.
 */
#include "can_task.h"
#include "can/app_gateway.h"
#include "can/app_can_bitrate.h"
#include "et100_fdb_keys.h"
#include "system_status.h"
#include <string.h>
#ifndef ET100_CAN_HOST_TEST
#include "fr30xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fdb_app.h"
#else
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
/* Host supplies the same hardware/storage boundary, without an EVB. */
int et100_can_hw_apply(unsigned kbps);
int et100_can_hw_bus_off(void);
int et100_can_record_read(uint8_t record[16]);
int et100_can_record_write(const uint8_t record[16]);
#endif

struct rx_frame { uint32_t id, epoch; uint8_t data[8], dlc; };
struct tx_line { char text[ET100_CAN_LINE_MAX]; uint16_t length; uint8_t report; };
static struct rx_frame ring[ET100_CAN_RING_FRAMES];
static volatile unsigned head, tail;
static CanRequest_t commands[8];
static unsigned command_head, command_tail;
static struct tx_line output[16];
static unsigned output_head, output_tail;
static struct tx_line deferred_boot;
static volatile uint32_t rx_epoch;
static uint32_t confidence_epoch;
static CanConfig_t configuration;
static CanBitrate_t bitrate;
static struct et100_can_diagnostics diagnostics;
static bool storage, paused, ready, recovery, recovery_applied, boot_sent;
static volatile bool error_pending, off_pending;
static uint32_t now_ms, boot_ms, recover_since, last_rx;
#ifndef ET100_CAN_HOST_TEST
static int et100_can_hw_apply(unsigned kbps);
static int et100_can_hw_bus_off(void);
static int et100_can_record_read(uint8_t record[16]);
static int et100_can_record_write(const uint8_t record[16]);
#endif

static void status(unsigned value)
{
    taskENTER_CRITICAL();diagnostics.status=(uint8_t)value;taskEXIT_CRITICAL();
    et100_sys_set(12,value);
}
static bool enqueue(const char *line,size_t length,uint8_t report)
{
    bool accepted=false;
    taskENTER_CRITICAL();
    unsigned next=(output_head+1u)%16u;
    if (length && length<ET100_CAN_LINE_MAX && next!=output_tail) {
        memcpy(output[output_head].text,line,length);
        output[output_head].length=(uint16_t)length;
        output[output_head].report=report;
        output_head=next;accepted=true;
    } else diagnostics.tx_dropped++;
    taskEXIT_CRITICAL();
    return accepted;
}
static void telemetry(const char *line,size_t length)
{
    taskENTER_CRITICAL();bool stop=paused;taskEXIT_CRITICAL();
    if (!stop) (void)enqueue(line,length,true);
}
void et100_can_set_paused(bool value)
{
    taskENTER_CRITICAL();paused=value;taskEXIT_CRITICAL();
}
size_t et100_can_take_line(char *line,size_t capacity,bool stop)
{
    size_t length=0;
    taskENTER_CRITICAL();
    if(!stop && deferred_boot.length) {
        if(capacity>deferred_boot.length) {
            length=deferred_boot.length;memcpy(line,deferred_boot.text,length);line[length]=0;
            deferred_boot.length=0;
        }
        taskEXIT_CRITICAL();return length;
    }
    while(output_tail!=output_head) {
        const struct tx_line *entry=&output[output_tail];
        if(entry->report==2u && stop) {
            deferred_boot=*entry;output_tail=(output_tail+1u)%16u;continue;
        }
        if(entry->report==1u && stop) {output_tail=(output_tail+1u)%16u;continue;}
        if(capacity<=entry->length)break;
        length=entry->length;memcpy(line,entry->text,length);line[length]=0;
        output_tail=(output_tail+1u)%16u;break;
    }
    taskEXIT_CRITICAL();return length;
}
void et100_can_diagnostics(struct et100_can_diagnostics *out)
{
    if(!out)return;
    taskENTER_CRITICAL();*out=diagnostics;taskEXIT_CRITICAL();
}
void et100_can_receive_isr(uint32_t id,const uint8_t *data,uint8_t dlc,
                           bool extended,bool remote,bool fd)
{
    if(!data || !extended || remote || fd || id>0x1fffffffu || dlc>8u) {diagnostics.rejected++;return;}
    /* Monotonic SPSC cursors permit all 64 slots; task consumes under critical. */
    if(head-tail>=ET100_CAN_RING_FRAMES) {diagnostics.overflow++;rx_epoch++;error_pending=true;return;}
    struct rx_frame *frame=&ring[head%ET100_CAN_RING_FRAMES];
    frame->id=id;frame->epoch=rx_epoch;frame->dlc=dlc;memcpy(frame->data,data,dlc);
    __atomic_thread_fence(__ATOMIC_RELEASE);head++;
    diagnostics.received++;
}
void et100_can_error_isr(bool bus_off,bool passive,uint8_t tec,uint8_t rec)
{
    rx_epoch++;error_pending=true;off_pending|=bus_off;diagnostics.errors++;
    diagnostics.tec=tec;diagnostics.rec=rec;diagnostics.passive=passive;
}
static void encode(const CanConfig_t *c,uint8_t record[16])
{
    memset(record,0,16);memcpy(record,"ECAN",4);record[4]=1;
    record[5]=c->types;record[6]=c->bitrate;
    record[8]=(uint8_t)c->gtime;record[9]=(uint8_t)(c->gtime>>8);
    record[10]=(uint8_t)c->cartype;record[11]=(uint8_t)(c->cartype>>8);
    for(unsigned i=0;i<14;i++)record[14]^=record[i];
    record[15]=(uint8_t)~record[14];
}
static bool decode(const uint8_t record[16],CanConfig_t *c)
{
    uint8_t check=0;
    for(unsigned i=0;i<14;i++)check^=record[i];
    if(memcmp(record,"ECAN",4)||record[4]!=1||record[7]||record[12]||record[13]||
       check!=record[14] || (unsigned)(record[14]^record[15])!=255)return false;
    *c=(CanConfig_t){.types=record[5],.bitrate=record[6],
        .gtime=(uint16_t)(record[8]|record[9]<<8),.cartype=(uint16_t)(record[10]|record[11]<<8)};
    return CanConfig_Valid(c)!=0;
}
static bool save(const CanConfig_t *c)
{
    uint8_t record[16],verify[16];encode(c,record);
    if(!storage)return false;
    if(et100_can_record_read(verify)&&!memcmp(record,verify,16))return true;
    return et100_can_record_write(record)&&et100_can_record_read(verify)&&!memcmp(record,verify,16);
}
static int apply(void *context,unsigned kbps,int silent)
{
    (void)context;(void)silent; /* D2: even a portable NORMAL request stays MON. */
    /* Locking an already monitored rate changes no hardware mode on ET-100.
     * Keep its FIFO and decoded epoch instead of dropping the lock frames. */
    if(bitrate.running&&!recovery&&diagnostics.kbps==kbps)return 1;
    if(!et100_can_hw_apply(kbps)) {
        taskENTER_CRITICAL();tail=head;taskEXIT_CRITICAL();
        AppGateway_Invalidate(now_ms);status(6);return 0;
    }
    taskENTER_CRITICAL();tail=head;diagnostics.kbps=kbps;taskEXIT_CRITICAL();
    AppGateway_Invalidate(now_ms);status(recovery?5:2);return 1;
}
static void initialize(int storage_ready,uint32_t now)
{
    uint8_t record[16];
    head=tail=command_head=command_tail=output_head=output_tail=0;
    rx_epoch=confidence_epoch=0;deferred_boot.length=0;
    memset(&diagnostics,0,sizeof diagnostics);
    storage=storage_ready!=0;ready=false;recovery=recovery_applied=boot_sent=paused=false;
    error_pending=off_pending=false;now_ms=boot_ms=last_rx=now;
    configuration=CAN_CONFIG_DEFAULT;
    if(storage&&et100_can_record_read(record)) (void)decode(record,&configuration);
    if(!CanConfig_Valid(&configuration))configuration=CAN_CONFIG_DEFAULT;
    status(1);AppGateway_Init(telemetry,now,1);AppGateway_Configure(&configuration,now);
    CanBitrate_Init(&bitrate,apply,NULL);
    (void)CanBitrate_Set(&bitrate,configuration.bitrate,now);
    taskENTER_CRITICAL();ready=true;taskEXIT_CRITICAL();
}
size_t et100_can_command(const char *line,char *failure,size_t capacity)
{
    CanRequest_t request;
    if(!CanProtocol_Parse(line,&request))return 0;
    taskENTER_CRITICAL();
    unsigned next=(command_head+1u)%8u;
    if(ready&&next!=command_tail) {
        commands[command_head]=request;command_head=next;taskEXIT_CRITICAL();return 0;
    }
    CanConfig_t snapshot=configuration;taskEXIT_CRITICAL();
    return CanProtocol_Reply(&request,&snapshot,97,0,failure,capacity);
}
void et100_can_step(uint32_t now)
{
    if(!ready)return;
    now_ms=now;
    taskENTER_CRITICAL();bool error=error_pending,off=off_pending;
    error_pending=off_pending=false;taskEXIT_CRITICAL();
    if(error)CanBitrate_Observe(&bitrate,now,0);
    if(off) {
        diagnostics.bus_off++;recovery=true;recovery_applied=false;recover_since=now;
        AppGateway_Invalidate(now);status(4);
    } else if(recovery) {
        status(5);
        if(!recovery_applied&&(uint32_t)(now-recover_since)>=100u) {
            bitrate.running=0;bitrate.last_attempt=now-500u;
            CanBitrate_Poll(&bitrate,now);recovery_applied=bitrate.running!=0;
            recover_since=now;
        }
        /* M_CAN clears BO only after bus recovery (129 occurrences of 11
         * recessive bits). INIT release alone must not claim recovery. */
        if(recovery_applied&&!et100_can_hw_bus_off()){recovery=false;status(2);}
    }
    /* Reserve an output slot before removing a command; never lose a reply. */
    taskENTER_CRITICAL();
    bool command=command_tail!=command_head && (output_head+1u)%16u!=output_tail;
    CanRequest_t request;
    if(command){request=commands[command_tail];command_tail=(command_tail+1u)%8u;}
    taskEXIT_CRITICAL();
    if(command) {
        bool ok=request.valid!=0;
        CanConfig_t next;
        if(request.setter) {
            ok=ok&&CanProtocol_Candidate(&request,&configuration,&next);
            CanBitrate_t old=bitrate;
            if(ok&&next.bitrate!=configuration.bitrate)ok=CanBitrate_Set(&bitrate,next.bitrate,now)!=0;
            if(ok)ok=save(&next);
            if(ok) {
                taskENTER_CRITICAL();configuration=next;taskEXIT_CRITICAL();
                AppGateway_Configure(&configuration,now);
            }
            else if(bitrate.setting!=old.setting) {
                (void)CanBitrate_Set(&bitrate,old.setting,now);
            }
        }
        char response[ET100_CAN_LINE_MAX];
        size_t length=CanProtocol_Reply(&request,&configuration,97,ok,response,sizeof response);
        (void)enqueue(response,length,false);
    }
    if(!recovery) {
        for(unsigned i=0;i<ET100_CAN_RING_FRAMES;i++) {
            struct rx_frame frame;
            taskENTER_CRITICAL();bool available=tail!=head;
            uint32_t epoch=rx_epoch;
            if(available){__atomic_thread_fence(__ATOMIC_ACQUIRE);frame=ring[tail%ET100_CAN_RING_FRAMES];tail++;}
            taskEXIT_CRITICAL();if(!available)break;
            if(confidence_epoch!=epoch) {
                CanBitrate_Observe(&bitrate,now,0);confidence_epoch=epoch;
            }
            AppGateway_Frame(frame.id,frame.data,frame.dlc,now);
            if(frame.epoch==epoch)CanBitrate_Observe(&bitrate,now,1);
            last_rx=now;
            if(bitrate.running)status(3);
        }
        CanBitrate_Poll(&bitrate,now);
        if(bitrate.running&&(uint32_t)(now-last_rx)>=3000u)status(2);
    }
    diagnostics.locked=bitrate.locked;
    AppGateway_Poll(now); /* Decode/expire/sample even while UART is paused. */
    taskENTER_CRITICAL();bool stop=paused;taskEXIT_CRITICAL();
    if(!boot_sent&&!stop&&(uint32_t)(now-boot_ms)>=300000u) {
        char line[ET100_CAN_LINE_MAX];
        size_t length=AppGateway_BootLine(&configuration,97,line,sizeof line);
        boot_sent=enqueue(line,length,2);
    }
}

#ifdef ET100_CAN_HOST_TEST
void et100_can_start(int storage_ready) {initialize(storage_ready,0);}
#else
static CAN_HandleTypeDef can;
/* SDK also reserves 64 unused dedicated RX elements (fixed SDK constant).
 * 8 + 64*16 + 64*16 = 2056 bytes. No TX RAM or transmit API. */
static uint32_t message_ram[514] __attribute__((aligned(4)));
static int et100_can_record_read(uint8_t record[16])
{
    return flashdb_get_length(ET100_FDB_CAN_KEY)==16&&flashdb_get(ET100_FDB_CAN_KEY,record,16)==16;
}
static int et100_can_record_write(const uint8_t record[16])
{
    return flashdb_set(ET100_FDB_CAN_KEY,(void *)record,16)==FDB_NO_ERR;
}
static int et100_can_hw_bus_off(void) {return can.CANx->ProtocolStatus.Bits.BO!=0;}
static int et100_can_hw_apply(unsigned kbps)
{
    NVIC_DisableIRQ(CAN3_Line0_IRQn);
    __SYSTEM_GPIOA_CLK_ENABLE();__SYSTEM_MCAN3_CLK_ENABLE();
    __SYSTEM_MCAN_CLK_SELECT_D48M();__SYSTEM_MCAN_CLK_DIV(2);
    const uint32_t clock=system_get_peripheral_clock(PER_CLK_CANx);
    if((kbps!=250u&&kbps!=500u)||!clock||clock%(kbps*1000u*24u))return 0;
    GPIO_InitTypeDef pins={.Pin=GPIO_PIN_10|GPIO_PIN_11,.Mode=GPIO_MODE_AF_PP,
        .Pull=GPIO_PULLUP,.Alternate=GPIO_FUNCTION_5};gpio_init(GPIOA,&pins);
    can.CANx=CAN3;
    __CAN_INIT_START(can.CANx);
    unsigned wait=100000u;while(!can.CANx->CCCtrl.INIT&&--wait){}
    if(!wait)return 0;
    __CAN_CHANGE_ENABLE(can.CANx);
    __CAN_BUS_MONITOR_MODE_ENABLE(can.CANx);
    __CAN_SET_NOMINAL_RATE_PRESCALER(can.CANx,clock/(kbps*1000u*24u));
    /* SDK segment fields are raw (actual = field+1): 1+20+3 TQ, SP=87.5%. */
    __CAN_SET_NOMINAL_TIME_SEG_1(can.CANx,19);
    __CAN_SET_NOMINAL_TIME_SEG_2(can.CANx,2);
    __CAN_SET_NOMINAL_SYNC_WIDTH(can.CANx,2);
    __CAN_FD_MODE_DISABLE(can.CANx);__CAN_BIT_RATE_SWITCH_DISABLE(can.CANx);
    __CAN_AUTOMATIC_RETRANSMISSION_DISABLE(can.CANx);
    can.RAMConfig=(struct_RAMConfig_t){0};
    can.RAMConfig.StartAddress=(uint32_t)message_ram;
    can.RAMConfig.ExtendedIDFilterNums=1;can.RAMConfig.RxFIFO0Nums=64;
    can.RAMConfig.DataBufferSize=CAN_DATA_BUFFER_SIZE_8_BYTE;
    memset(message_ram,0,sizeof message_ram);
    if(can_message_ram_init(&can)>sizeof message_ram)return 0;
    /* INIT requests cross clock domains. Let the SDK's exit finish before
     * requesting entry again; back-to-back requests need not be acknowledged. */
    wait=100000u;while(can.CANx->CCCtrl.INIT&&--wait){}
    if(!wait)return 0;
    /* Re-enter before protected filter registers. */
    __CAN_INIT_START(can.CANx);
    wait=100000u;while(!can.CANx->CCCtrl.INIT&&--wait){}
    if(!wait)return 0;
    __CAN_CHANGE_ENABLE(can.CANx);
    struct_FilterCfg_t filter={.FilterType=CAN_FILTER_RANGE_FILTER,
        .ProcessMode=FILTER_PROCESS_STORE_IN_RxFIFO0,.FilterID_1=0,.FilterID_2=0x1fffffffu};
    can_add_extended_filter(&can,filter,0);
    __CAN_STANDARD_REJECT_UNMATCHED_FRAME(can.CANx);__CAN_STANDARD_REJECT_REMOTE_FRAME(can.CANx);
    __CAN_EXTENDED_REJECT_UNMATCHED_FRAME(can.CANx);__CAN_EXTENDED_REJECT_REMOTE_FRAME(can.CANx);
    can_clear_int_status(&can,0xffffffffu);
    const uint32_t interrupts=INT_RxFIFO0_NEW_MESSAGE|INT_RxFIFO0_MESSAGE_LOST|INT_ERROR_PASSIVE|INT_BUS_OFF_STATUS;
    can_int_select_line(&can,interrupts,CAN_INT_LINE0);
    can_int_enable(&can,interrupts);__CAN_INT_LINE_ENABLE(can.CANx);
    __CAN_CHANGE_DISABLE(can.CANx);__CAN_INIT_STOP(can.CANx);
    /* MON was set while INIT/CCE were acknowledged. Do not call the SDK's
     * monitoring helper here: it requests INIT again without waiting for the
     * previous exit. DAP showed INIT=1/MON=1 while our old check claimed ready. */
    wait=100000u;while(can.CANx->CCCtrl.INIT&&--wait){}
    if(!wait||!can.CANx->CCCtrl.MON||can.CANx->CCCtrl.CCE||
       can.CANx->CCCtrl.FDOE||can.CANx->CCCtrl.BRSE)return 0;
    diagnostics.clock_hz=clock;diagnostics.prescaler=clock/(kbps*1000u*24u);
    diagnostics.seg1=20;diagnostics.seg2=3;
    NVIC_SetPriority(CAN3_Line0_IRQn,4);NVIC_EnableIRQ(CAN3_Line0_IRQn);
    return 1;
}
void can3_line0_irq(void)
{
    const uint32_t pending=can_get_int_status(&can);
    can_clear_int_status(&can,pending);
    if(pending&(INT_RxFIFO0_MESSAGE_LOST|INT_ERROR_PASSIVE|INT_BUS_OFF_STATUS))
        et100_can_error_isr((pending&INT_BUS_OFF_STATUS)&&can.CANx->ProtocolStatus.Bits.BO,
            can.CANx->ProtocolStatus.Bits.EP,can_get_transmit_error_counter(&can),can_get_receive_error_counter(&can));
    for(unsigned i=0;i<64u&&can_get_rxfifo0_fill_level(&can);i++) {
        struct_CANRxHeaderDef_t header;uint8_t data[64];
        (void)can_get_rxfifo0_message(&can,&header,data);
        et100_can_receive_isr(header.Identifier,data,header.DLC,header.IdType==CAN_ID_EXTENDED,
            header.FrameType!=CAN_DATA_FRAME,header.FormatMode!=CAN_CLASSICAL_FRAME);
    }
}
static void can_owner(void *argument)
{
    initialize((int)(uintptr_t)argument,(uint32_t)xTaskGetTickCount()*portTICK_PERIOD_MS);
    for(;;){et100_can_step((uint32_t)xTaskGetTickCount()*portTICK_PERIOD_MS);vTaskDelay(pdMS_TO_TICKS(50));}
}
void et100_can_start(int storage_ready)
{
    configASSERT(xTaskCreate(can_owner,"can",1024,(void *)(uintptr_t)(storage_ready!=0),1,NULL)==pdPASS);
}
#endif
