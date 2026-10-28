/* Source: eup_canbus_32f407/app/app_gateway.c, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    app_gateway.c
 * @brief   Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details
 * New implementation based on EVB wire behavior; generated CSV rules define PGN/SPN locations.
 * - V1.9: CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 * 2026-10-28  Codex  Implement S3 telemetry modes using EVB wire behavior as specification.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#include "app_gateway.h"
#include "j1939_sheet_tables_gen.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdatomic.h>
#define LINE_SIZE 160U
static AppGatewayOutput_t sink;
static AppGatewayMode_t mode;
static CanConfig_t config;
static struct { uint32_t raw, last; uint8_t seen, valid; } fields[FIELD_COUNT];
static uint32_t last_type2, last_burst, last_sample;
static struct { uint8_t speed, throttle, brake; uint16_t rpm; uint32_t steer; } history[10];
static unsigned history_next, history_count;
static uint8_t reported, last_state[3];
static _Atomic uint8_t snapshot;
/* Persistent scratch avoids stacking multiple line buffers on the 512-word CAN task. */
static char line[LINE_SIZE], text[8][32], payload3[101], payload4[51];

/******************************************************************************
 * @brief   AppGateway_Wire — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure reentrant formatter; validates capacity before writing, XOR covers body only; no mutable global scratch.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Check body/output/capacity, XOR the body bytes, and format prefix/body/two-digit XOR/CRLF plus NUL.
 * @edge_case NULL or short output fails with 0 and clears an available buffer; neither prefix nor CRLF participates in XOR.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
size_t AppGateway_Wire(const char *body, char *out, size_t capacity)
{
    if (!body || !out || capacity == 0U) { return 0U; }
    out[0]='\0';
    size_t n=strlen(body);
    if (n+12U>capacity) { return 0U; } /* prefix 6 + star/checksum 3 + CRLF 2 + NUL */
    uint8_t checksum=0U;
    for (size_t i=0U;i<n;i++) { checksum^=(uint8_t)body[i]; }
    int length=snprintf(out,capacity,"AT$PD=%s*%02X\r\n",body,(unsigned)checksum);
    return length>0 && (size_t)length<capacity ? (size_t)length : 0U;
}

/******************************************************************************
 * @brief   Format and emit a complete bounded line.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Owner-task call; formats into persistent scratch, then invokes the copying callback.
 * - V1.9: CAN task only; bounded static scratch appends XOR to official CAN bodies; other S3 modes retain diagnostic formatting.
 * @input   format and scalar arguments.
 * @output  At most 159 bytes per call.
 * @algorithm Format persistent line scratch, append XOR only for official AT$PD bodies, then invoke the copying sink with the complete length.
 * @edge_case NULL sink, truncation or insufficient suffix space suppresses the entire line; diagnostic modes keep their own CRLF.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static void emit(const char *format, ...)
{
    va_list args;va_start(args,format);
    int length=vsnprintf(line,sizeof(line),format,args);va_end(args);
    if (!sink || length<=0 || (size_t)length>=sizeof(line)) { return; }
    if (strncmp(line,"AT$PD=",6U)==0)
    {
        /* Official bodies are formatted without CRLF; append XOR in-place with bounded scratch. */
        uint8_t cs=0U;
        for (int i=6;i<length;i++) { cs^=(uint8_t)line[i]; }
        if ((size_t)length+6U>sizeof(line)) { return; }
        int suffix=snprintf(line+length,sizeof(line)-(size_t)length,"*%02X\r\n",cs);
        if (suffix!=5) { return; }
        length+=suffix;
    }
    sink(line,(size_t)length);
}

/******************************************************************************
 * @brief   integer — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure bounded int64 decimal conversion avoids unsupported nano printf long-long formats; values are bounded by four-byte SPNs.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Extract base-10 digits into reverse scratch, write an optional minus, then reverse-copy and terminate.
 * @edge_case Callers provide a 32-byte destination and SPN-bounded signed values; nano printf long-long support is unnecessary.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static void integer(char *out, int64_t number)
{
    char digits[20];unsigned n=0U,at=0U;
    uint64_t magnitude=(uint64_t)(number<0 ? -number : number);
    do { digits[n++]=(char)('0'+magnitude%10U);magnitude/=10U; } while(magnitude);
    if(number<0) { out[at++]='-'; }
    while(n) { out[at++]=digits[--n]; }
    out[at]='\0';
}

/******************************************************************************
 * @brief   temperature — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Truncate signed numerator by its positive divisor, emit sign 0/1, then format the absolute integer magnitude.
 * @edge_case Negative fractions truncating to zero use positive zero; callers supply nonzero divisors 1 or 32 and 32-byte output.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static void temperature(char *out,int32_t numerator,unsigned divisor)
{
    unsigned magnitude=(unsigned)(numerator<0 ? -numerator : numerator)/divisor;
    out[0]=numerator<0 && magnitude!=0U ? '0':'1'; /* D14: negative fractional zero is positive zero. */
    integer(out+1,magnitude);
}

/******************************************************************************
 * @brief   AppGateway_RequestSnapshot — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Link or CAN task may atomically request a future Type1 snapshot without holding the app mutex; no field/history mutation or blocking.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Atomically set a pending snapshot flag for the CAN owner to consume on its next Type1 opportunity.
 * @edge_case Repeated requests coalesce; a USB/link caller never mutates field/history caches.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_RequestSnapshot(void) { atomic_store_explicit(&snapshot,1U,memory_order_release); }

/******************************************************************************
 * @brief   AppGateway_Invalidate — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Clear all field validity and history, reset sample deadline/count and force a future Type1 state report.
 * @edge_case A stop/rate change cannot carry samples into the next bus epoch; raw storage is not a substitute for valid flags.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_Invalidate(uint32_t now)
{
    memset(fields,0,sizeof(fields));history_next=history_count=0U;
    last_sample=now-1000U;reported=0U;AppGateway_RequestSnapshot();
}


/* Retired EVB helpers: historical authorship retained; V1.9 no longer calls these algorithms.
 * Retired helper: hhmmss (replaced by integer/raw V1.9 encoding).
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 * Retired helper: rounded (replaced by integer/raw V1.9 encoding).
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 * Retired helper: fixed_text (replaced by integer/raw V1.9 encoding).
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 * Retired helper: field_text (replaced by integer/raw V1.9 encoding).
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 * Retired helper: decode (replaced by integer/raw V1.9 encoding).
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 * Retired helper: update_engine (replaced by integer/raw V1.9 encoding).
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 * Retired helper: packed (replaced by integer/raw V1.9 encoding).
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 */
/******************************************************************************
 * @brief   Initialize portable telemetry.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details
 * - V1.9: CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   copying callback, current tick, Type4 enable.
 * @output  Default RS232 mode.
 * @algorithm Clear fields/history and report state, install sink/defaults, reset deadline baselines and select RS232 mode.
 * @edge_case Startup is serialized; NULL sink suppresses output and CAN clock strings remain 000000 without a time source.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_Init(AppGatewayOutput_t output,uint32_t now,uint8_t type4)
{
    sink=output;mode=APP_GATEWAY_RS232;
    config=(CanConfig_t){1U,30U,type4 ? 15U:7U,0U};
    atomic_init(&snapshot,0U);last_type2=last_burst=now;
    AppGateway_Invalidate(now);
}

/******************************************************************************
 * @brief   AppGateway_Configure — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Reset the Type2 deadline only when GTIME changes, copy the configuration and request a Type1 snapshot.
 * @edge_case Caller supplies a validated configuration under app mutex; Type3/4 cadence is not reset by GTIME.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_Configure(const CanConfig_t *next,uint32_t now)
{
    if (!next || next->gtime<1U || next->gtime>9999U || next->cartype!=1U || next->types>15U || next->bitrate>2U) { return; }
    if (config.gtime!=next->gtime) { last_type2=now; }
    if (config.types!=next->types) { last_burst=now; }
    config=*next;AppGateway_RequestSnapshot();
}

/******************************************************************************
 * @brief   Return the currently selected S3 mode.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Owner-task or app-mutex caller only; read-only and nonblocking.
 * - V1.9: CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   None.
 * @output  Selected mode.
 * @algorithm Return the current mode without changing state or deadlines.
 * @edge_case Owner/app-mutex read only; the startup mode is RS232.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
AppGatewayMode_t AppGateway_Mode(void) { return mode; }

/******************************************************************************
 * @brief   Cycle S3 output mode and report it.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN task under app mutex; queue an EVB-compatible mode message.
 * - V1.9: CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   None.
 * @output  Selected mode and mode status line.
 * @algorithm Advance the three-mode enum modulo three, request a snapshot when entering RS232 and enqueue the mode notice.
 * @edge_case Caches and cadence remain intact; transport routes the notice to USB only, keeping UART product output clean.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
AppGatewayMode_t AppGateway_NextMode(void)
{
    static const char *names[]={"RS232","J1939","ASCII"};
    mode=(AppGatewayMode_t)(((unsigned)mode+1U)%3U);
    if(mode==APP_GATEWAY_RS232) { AppGateway_RequestSnapshot(); }
    emit("[ Mode ] : %s\r\n",names[mode]);
    return mode;
}

/******************************************************************************
 * @brief   extract — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Check the field width against DLC, assemble little-endian raw bytes, apply the bit slice and test explicit invalid enumeration masks.
 * @edge_case Short DLC immediately fails; numeric all-ones remains valid and four-byte assembly never shifts by 32.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static int extract(const GatewayRule_t *rule,const uint8_t *data,uint8_t dlc,uint32_t *raw)
{
    if(rule->byte+rule->bytes>dlc) { return 0; }
    *raw=0U;
    for(unsigned i=0;i<rule->bytes;i++) { *raw|=(uint32_t)data[rule->byte+i]<<(8U*i); }
    if(rule->bits) { *raw=(*raw>>rule->bit)&((1U<<rule->bits)-1U); }
    return !(*raw<32U && (rule->invalid_mask&(1U<<*raw)));
}

/******************************************************************************
 * @brief   ascii_value — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Compute rational physical units in int64 thousandths, format integer/fraction and append the field-specific diagnostic unit.
 * @edge_case Invalid fields emit status/dash markers; negative values between -1 and 0 retain their minus and no float is used.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static void ascii_value(unsigned field)
{
    const GatewayRule_t *rule=&gateway_rules[field];
    if(!fields[field].valid) { emit("SPN%lu=%s\r\n",(unsigned long)rule->spn,rule->bits ? "-":"--");return; }
    int64_t numerator=(int64_t)fields[field].raw*rule->numerator+rule->offset;
    int64_t milli=(numerator*1000+(numerator<0 ? -rule->denominator/2:rule->denominator/2))/rule->denominator;
    if(milli%1000==0)
    { integer(text[7],milli/1000); }
    else
    {
        integer(text[7],milli/1000);
        if(milli<0 && milli/1000==0) { strcpy(text[7],"-0"); }
        size_t n=strlen(text[7]);unsigned fraction=(unsigned)(milli<0 ? -milli:milli)%1000U;
        (void)snprintf(text[7]+n,sizeof(text[7])-n,".%03u",fraction);
    }
    /* Physical units remain a diagnostic mode, not the integer V1.9 report encoding. */
    const char *unit=field==FIELD_STARTER || field==FIELD_HANDBRAKE ? "Status":field==FIELD_GEAR ? "Gear":
        field==FIELD_RPM ? "rpm":field==FIELD_SPEED ? "km/h":field==FIELD_MILEAGE ? "km":
        field==FIELD_WATER || field==FIELD_OIL || field==FIELD_UREA_TEMP ? "degC":
        field==FIELD_STEER ? "rad":field==FIELD_ECONOMY ? "km/L":field==FIELD_TOTAL_FUEL ? "L":"%";
    emit("SPN%lu=%s%s\r\n",(unsigned long)rule->spn,text[7],unit);
}

/******************************************************************************
 * @brief   Decode and route an extended CAN2 data frame.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Cache known fields in every mode; J1939 emits raw CSV, ASCII emits every matching rule.
 * - V1.9: CAN owner processes only complete mapped fields; DLC or enumeration errors invalidate immediately, all-ones numeric values remain valid.
 * @input   29-bit ID, valid data pointer/DLC, receive tick.
 * @output  Decoded cache plus selected per-frame output.
 * @algorithm Validate ID/DLC, derive normalized PGN, optionally emit raw CSV, then extract every mapped field and route ASCII values.
 * @edge_case Bad DLC/enum invalidates that field immediately; unknown PGNs are raw-only and valid all-ones numerical SPNs are retained.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_Frame(uint32_t id,const uint8_t *data,uint8_t dlc,uint32_t now)
{
    if(!data || dlc>8U || id>0x1FFFFFFFU) { return; }
    uint32_t pgn=(id>>8)&0x3FFFFU;
    if(((id>>16)&255U)<240U) { pgn&=~255U; }
    if(mode==APP_GATEWAY_J1939)
    {
        uint8_t bytes[8]={0};memcpy(bytes,data,dlc);
        emit("%08lX,%u,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\r\n",(unsigned long)id,dlc,
             bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7]);
    }
    for(unsigned i=0;i<FIELD_COUNT;i++)
    {
        if(gateway_rules[i].pgn!=pgn) { continue; }
        uint32_t raw=0U;fields[i].seen=1U;
        fields[i].valid=(uint8_t)extract(&gateway_rules[i],data,dlc,&raw);
        fields[i].raw=raw;
        if(fields[i].valid) { fields[i].last=now; }
        if(mode==APP_GATEWAY_ASCII) { ascii_value(i); }
    }
}

/******************************************************************************
 * @brief   Encode Type1 changes.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details
 * - V1.9: CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   None; owner-task globals.
 * @output  EVB Type1 line with empty unavailable fields.
 * @algorithm Build raw state plus separate validity bits, consume snapshot request, compare the last emitted state and format changed/initial fields.
 * @edge_case Gear 255 is valid despite matching the byte sentinel; all-missing initial/expired states still emit empty fields.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static void type1(void)
{
    uint8_t state[3]={255U,255U,255U};
    const unsigned keys[]={FIELD_STARTER,FIELD_GEAR,FIELD_HANDBRAKE};
    for(unsigned i=0;i<3U;i++) { if(fields[keys[i]].valid) { state[i]=(uint8_t)fields[keys[i]].raw; } }
    /* Gear 255 is valid (D15); validity bits participate separately in the comparison. */
    static uint8_t last_valid;
    uint8_t validity=0U;
    for(unsigned i=0;i<3U;i++) { validity|=(uint8_t)(fields[keys[i]].valid<<i); }
    if(atomic_exchange_explicit(&snapshot,0U,memory_order_acq_rel)) { reported=0U; }
    if(reported && validity==last_valid && !memcmp(state,last_state,sizeof(state))) { return; }
    for(unsigned i=0;i<3U;i++)
    { text[i][0]='\0';if(validity&(1U<<i)) { integer(text[i],state[i]); } }
    emit("AT$PD=67;1;000000;%s;%s;%s",text[0],text[1],text[2]);
    memcpy(last_state,state,sizeof(state));last_valid=validity;reported=1U;
}

/******************************************************************************
 * @brief   Encode the configured-period Type2 snapshot.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details
 * - V1.9: CAN-owner context under app mutex; nonblocking copied output, integer-only conversion and wrap-safe TTL. No CAN time source means 000000.
 * @input   None; owner-task cached state.
 * @output  One complete Type2 line.
 * @algorithm Clear eight field strings, independently format valid integer/temperature values and emit V1.9 order with the urea colon.
 * @edge_case Mileage truncates and levels round; total fuel uses 64-bit multiplication; partial/missing urea retains its colon.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static void type2(void)
{
    const unsigned keys[]={FIELD_MILEAGE,FIELD_FUEL,FIELD_ECONOMY,FIELD_WATER,FIELD_OIL,FIELD_UREA,FIELD_UREA_TEMP,FIELD_TOTAL_FUEL};
    for(unsigned i=0;i<8U;i++)
    {
        text[i][0]='\0';unsigned key=keys[i];uint32_t raw=fields[key].raw;
        if(!fields[key].valid) { continue; }
        if(key==FIELD_WATER || key==FIELD_OIL) { temperature(text[i],(int32_t)raw-8736,32U); }
        else if(key==FIELD_UREA_TEMP) { temperature(text[i],(int32_t)raw-40,1U); }
        else
        {
            uint64_t value=raw;
            if(key==FIELD_MILEAGE) { value=(uint64_t)raw*5U/1000U; }
            if(key==FIELD_FUEL || key==FIELD_UREA) { value=(raw*4U+5U)/10U; }
            if(key==FIELD_TOTAL_FUEL) { value=(uint64_t)raw*5U; }
            integer(text[i],(int64_t)value);
        }
    }
    emit("AT$PD=67;2;000000;%s;%s;%s;%s;%s;%s:%s;%s",text[0],text[1],text[2],text[3],text[4],text[5],text[6],text[7]);
}

/******************************************************************************
 * @brief   Capture one software-second sample.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Insert latest fields in a ten-slot circular history; does not output or block.
 * - V1.9: CAN owner captures current valid fields once; missing elapsed seconds are all-F and no source value is saturated.
 * @input   None; cached software time and fields.
 * @output  Newest history slot and saturating sample count.
 * @algorithm Initialize the next circular slot to sentinels, optionally convert current valid fields, then advance the bounded ten-slot history.
 * @edge_case Missing seconds insert empty slots; speed/RPM use low-bit casts, not saturation; steering missing uses UINT32_MAX.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static void sample(unsigned empty)
{
    unsigned idx=history_next;
    history[idx].speed=history[idx].throttle=history[idx].brake=255U;
    history[idx].rpm=65535U;history[idx].steer=UINT32_MAX;
    if(!empty)
    {
        if(fields[FIELD_SPEED].valid) { history[idx].speed=(uint8_t)((fields[FIELD_SPEED].raw+128U)/256U); }
        if(fields[FIELD_RPM].valid) { history[idx].rpm=(uint16_t)((fields[FIELD_RPM].raw+4U)/8U); }
        if(fields[FIELD_THROTTLE].valid) { history[idx].throttle=(uint8_t)((fields[FIELD_THROTTLE].raw*4U+5U)/10U); }
        if(fields[FIELD_BRAKE].valid) { history[idx].brake=(uint8_t)((fields[FIELD_BRAKE].raw*4U+5U)/10U); }
        if(fields[FIELD_STEER].valid)
        {
            int32_t value=(int32_t)fields[FIELD_STEER].raw-32127;
            history[idx].steer=(value<0 ? 0U:0x10000U)|(uint32_t)(value<0 ? -value:value);
        }
    }
    history_next=(history_next+1U)%10U;if(history_count<10U) { history_count++; }
}

/******************************************************************************
 * @brief   Encode newest-to-oldest Type3/4 history.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Fixed ten-second cadence; at most ten samples and two complete lines per call.
 * - V1.9: CAN owner serializes up to ten newest-first samples; T3 uses ten hex digits, T4 five, within fixed buffers.
 * @input   None; Type4 enable and circular history.
 * @output  Compact uppercase hex payloads matching EVB field ordering.
 * @algorithm Walk at most ten history entries newest-first, build fixed-width Type3/4 payloads and emit only enabled types.
 * @edge_case Type3 uses ten hex characters per slot; Type4 uses five including sign, or FFFFF when unavailable.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
static void burst(void)
{
    for(unsigned i=0;i<history_count;i++)
    {
        unsigned idx=(history_next+9U-i)%10U;
        (void)snprintf(payload3+10U*i,sizeof(payload3)-10U*i,"%02X%04X%02X%02X",history[idx].speed,history[idx].rpm,history[idx].throttle,history[idx].brake);
        (void)snprintf(payload4+5U*i,sizeof(payload4)-5U*i,"%05lX",(unsigned long)(history[idx].steer==UINT32_MAX ? 0xFFFFFU:history[idx].steer));
    }
    if(config.types&4U) { emit("AT$PD=67;3;000000;%u;%s",history_count,payload3); }
    if(config.types&8U) { emit("AT$PD=67;4;000000;%u;%s",history_count,payload4); }
}

/******************************************************************************
 * @brief   Service TTL, sample and report deadlines.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details
 * - V1.9: CAN owner expires individual fields at TTL, inserts all-F missed slots, then emits type-masked change/periodic reports without blocking.
 * @input   now: millisecond tick.
 * @output  RS232 change and periodic lines; other modes only update history.
 * @algorithm Expire fields by TTL, insert missed-second empty samples plus one current sample, then service enabled RS232 change and periodic deadlines.
 * @edge_case At most ten slots are filled after a long pause; unsigned deltas handle wrap and no stale value is copied into missed seconds.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_Poll(uint32_t now)
{
    for(unsigned i=0;i<FIELD_COUNT;i++)
    { if(fields[i].valid && (uint32_t)(now-fields[i].last)>=gateway_rules[i].ttl_ms) { fields[i].valid=0U; } }
    uint32_t elapsed=now-last_sample;
    if(elapsed>=1000U)
    {
        uint32_t steps=elapsed/1000U;last_sample+=steps*1000U;
        if(steps>10U) { steps=10U; }
        while(steps) { sample(steps>1U);steps--; } /* Do not fabricate values for missed seconds. */
    }
    if(mode!=APP_GATEWAY_RS232) { return; }
    if(config.types&1U) { type1(); }
    if((uint32_t)(now-last_type2)>=(uint32_t)config.gtime*1000U)
    { last_type2=now;if(config.types&2U) { type2(); } }
    if((uint32_t)(now-last_burst)>=10000U) { last_burst=now;burst(); }
}

/******************************************************************************
 * @brief   AppGateway_BootLine — Exact V1.9 integer telemetry and validity.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Link task/host pure reentrant formatter using local scratch; reads caller-owned settings and does not access gateway globals or hardware.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Format numeric hardware/version/GTIME/type bits/setting bitrate into a local body and pass it to the reentrant wire formatter.
 * @edge_case NULL settings or a mask above 15 returns 0; output is complete with checksum/CRLF or suppressed if capacity is short.
 * @history
 * Retains the original telemetry-engine provenance; this helper was added for V1.9.
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
size_t AppGateway_BootLine(const CanConfig_t *settings,unsigned version,char *out,size_t capacity)
{
    char body[80];
    if(!settings || settings->types>15U) { return 0U; }
    (void)snprintf(body,sizeof(body),"67;0;V1;%u;%u;%u:%u:%u:%u;%u",version,settings->gtime,
        !!(settings->types&1U),!!(settings->types&2U),!!(settings->types&4U),!!(settings->types&8U),settings->bitrate);
    return AppGateway_Wire(body,out,capacity);
}
