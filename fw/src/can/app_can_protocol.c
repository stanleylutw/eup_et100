/* Source: eup_canbus_32f407/app/app_can_protocol.c, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    app_can_protocol.c
 * @brief   V1.9 CAN command validation and replies.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure bounded host/task code; no storage or locks. Checksums cover CAN$ body, replies never expose truncation.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#include "app_can_protocol.h"
#include "app_gateway.h"
#include <string.h>
#include <stdio.h>
const CanConfig_t CAN_CONFIG_DEFAULT = {1U, 30U, 15U, 0U};
/******************************************************************************
 * @brief   CanConfig_Valid — V1.9 CAN command validation and replies.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure field constraints for the only defined Cartype, reporting interval, type mask and bitrate modes.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Check the pointer, Cartype 1, GTIME 1..9999, four-bit type mask and bitrate enum in order.
 * @edge_case NULL configurations and any unsupported field fail without modifying memory.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int CanConfig_Valid(const CanConfig_t *c)
{
    return c && c->cartype == 1U && c->gtime >= 1U && c->gtime <= 9999U && c->types <= 15U &&
           c->bitrate <= 2U;
}
/******************************************************************************
 * @brief   hex_value — V1.9 CAN command validation and replies.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure ASCII hex conversion, accepting either case; invalid input returns -1.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Test decimal, uppercase A..F and lowercase a..f ranges; subtract the matching ASCII base.
 * @edge_case Any other character returns -1; no locale, time or buffer access is involved.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static int hex_value(char c)
{
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                  : -1;
}
/******************************************************************************
 * @brief   number — V1.9 CAN command validation and replies.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure bounded decimal parser; at most four digits avoids overflow and matches 1..9999 parameter limits.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Accumulate decimal digits with a pre-multiply 9999 bound; write the result only after the entire string passes.
 * @edge_case Empty strings, signs, nondigits and values above 9999 fail without accepting a prefix.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static int number(const char *text, uint16_t *out)
{
    unsigned n = 0;
    size_t length = strlen(text);
    if (!length || length > 4U)
    {
        return 0;
    }
    for (size_t i = 0; i < length; i++)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return 0;
        }
        n = n * 10U + (unsigned)(text[i] - '0');
    }
    *out = (uint16_t)n;
    return 1;
}
/******************************************************************************
 * @brief   CanProtocol_Parse — V1.9 CAN command validation and replies.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Task/host only; copies at most 260 ASCII bytes, checks exact token and optional XOR before setting a validated request; malformed framing stays silent.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Bound and copy printable ASCII, validate optional body XOR, split command/parameter/token, then validate command-specific fields.
 * @edge_case Bad framing/checksum/token returns 0; framed unknown or invalid commands return a request with valid=0 for a failure reply.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int CanProtocol_Parse(const char *line, CanRequest_t *r)
{
    if (!line || !r)
    {
        return 0;
    }
    memset(r, 0, sizeof(*r));
    r->command = CAN_CMD_UNKNOWN;
    char work[261];
    size_t length = 0;
    while (length < sizeof(work) && line[length])
    {
        unsigned char c = (unsigned char)line[length];
        if (c < 32U || c > 126U)
        {
            return 0;
        }
        length++;
    }
    if (length >= sizeof(work) || length < 10U || strncmp(line, "CAN$", 4U))
    {
        return 0;
    }
    memcpy(work, line, length + 1U);
    char *star = strchr(work, '*');
    if (star)
    {
        if (strlen(star) != 3U || hex_value(star[1]) < 0 || hex_value(star[2]) < 0)
        {
            return 0;
        }
        unsigned cs = 0;
        for (char *p = work; p < star; p++)
        {
            cs ^= (unsigned char)*p;
        }
        if (cs != (unsigned)(hex_value(star[1]) * 16 + hex_value(star[2])))
        {
            return 0;
        }
        *star = 0;
    }
    char *first = strchr(work + 4, ';'), *last = strrchr(work + 4, ';');
    if (!first || !last || strlen(last + 1) != 4U)
    {
        return 0;
    }
    for (unsigned i = 0; i < 4U; i++)
    {
        char c = last[1 + i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
        {
            return 0;
        }
    }
    size_t name_len = (size_t)(first - (work + 4));
    if (!name_len || name_len > 24U)
    {
        return 0;
    }
    for (size_t i = 0; i < name_len; i++)
    {
        char c = work[4 + i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
        {
            return 0;
        }
    }
    memcpy(r->name, work + 4, name_len);
    memcpy(r->token, last + 1, 4U);
    const char *name = r->name;
    const char *tail = name + 1;
    r->setter = name[0] == 'S';
    if (name[0] != 'S' && name[0] != 'G')
    {
        return 1;
    }
    if (!strcmp(tail, "TYPESWITCH"))
    {
        r->command = CAN_CMD_TYPES;
    }
    else if (!strcmp(tail, "GTIME"))
    {
        r->command = CAN_CMD_GTIME;
    }
    else if (!strcmp(tail, "CARTYPE"))
    {
        r->command = CAN_CMD_CARTYPE;
    }
    else if (!strcmp(tail, "BITRATE"))
    {
        r->command = CAN_CMD_BITRATE;
    }
    else if (!r->setter && !strcmp(tail, "VERSION"))
    {
        r->command = CAN_CMD_VERSION;
    }
    else
    {
        return 1;
    }
    if (!r->setter)
    {
        r->valid = first == last;
        return 1;
    }
    if (first == last)
    {
        return 1;
    }
    *last = 0;
    const char *param = first + 1;
    if (r->command == CAN_CMD_TYPES)
    {
        if (strlen(param) != 7U)
        {
            return 1;
        }
        for (unsigned i = 0; i < 4U; i++)
        {
            if ((param[i * 2] != '0' && param[i * 2] != '1') || (i < 3U && param[i * 2 + 1] != ':'))
            {
                return 1;
            }
            r->value |= (uint16_t)((unsigned)(param[i * 2] - '0') << i);
        }
        r->valid = 1U;
        return 1;
    }
    if (!number(param, &r->value))
    {
        return 1;
    }
    r->valid = (r->command == CAN_CMD_GTIME && r->value >= 1U) ||
               (r->command == CAN_CMD_CARTYPE && r->value == 1U) ||
               (r->command == CAN_CMD_BITRATE && r->value <= 2U);
    return 1;
}
/******************************************************************************
 * @brief   CanProtocol_Candidate — V1.9 CAN command validation and replies.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure copy on valid supported setters; unknown or invalid requests cannot mutate the running configuration.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Validate a setter and current configuration, copy the snapshot, replace the requested field, then validate the whole candidate.
 * @edge_case NULL pointers, getters, unknown commands or invalid values fail; no running configuration or storage is touched.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int CanProtocol_Candidate(const CanRequest_t *r, const CanConfig_t *current, CanConfig_t *next)
{
    if (!r || !current || !next || !r->setter || !r->valid || !CanConfig_Valid(current))
    {
        return 0;
    }
    *next = *current;
    switch (r->command)
    {
        case CAN_CMD_TYPES:
            next->types = (uint8_t)r->value;
            break;
        case CAN_CMD_GTIME:
            next->gtime = r->value;
            break;
        case CAN_CMD_CARTYPE:
            next->cartype = r->value;
            break;
        case CAN_CMD_BITRATE:
            next->bitrate = (uint8_t)r->value;
            break;
        default:
            return 0;
    }
    return CanConfig_Valid(next);
}
/******************************************************************************
 * @brief   CanProtocol_Reply — V1.9 CAN command validation and replies.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Task/host pure formatter; failure types echo only a syntactically valid requested mask, VERSION always carries numeric V1:number; success requires a valid configuration.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Select the success or specified failure payload, format command/value/token, then append body XOR and CRLF through AppGateway_Wire.
 * @edge_case STYPESWITCH failure echoes a valid requested mask; VERSION always includes identity; insufficient capacity returns 0.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
size_t CanProtocol_Reply(const CanRequest_t *r, const CanConfig_t *c, unsigned version, int success,
                         char *out, size_t capacity)
{
    if (!r || !out || !capacity)
    {
        return 0;
    }
    char value[40], body[100];
    success = success && r->valid && r->command != CAN_CMD_UNKNOWN;
    if (r->command != CAN_CMD_VERSION && !CanConfig_Valid(c))
    {
        success = 0;
    }
    if (r->command == CAN_CMD_VERSION)
    {
        (void)snprintf(value, sizeof(value), "V1:%u", version);
    }
    else if (r->command == CAN_CMD_TYPES)
    {
        unsigned types = success ? c->types : r->setter && r->valid ? r->value : 0U;
        (void)snprintf(value, sizeof(value), "%u:%u:%u:%u", !!(types & 1U), !!(types & 2U), !!(types & 4U),
                       !!(types & 8U));
    }
    else
    {
        unsigned number = 0U;
        if (success)
        {
            number = r->command == CAN_CMD_GTIME     ? c->gtime
                     : r->command == CAN_CMD_CARTYPE ? c->cartype
                     : r->command == CAN_CMD_BITRATE ? c->bitrate
                                                     : 0U;
        }
        (void)snprintf(value, sizeof(value), "%u", number);
    }
    int length = snprintf(body, sizeof(body), "CAN$%s;%u;%s;%s", r->name, success ? 1U : 0U, value, r->token);
    if (length < 0 || (size_t)length >= sizeof(body))
    {
        out[0] = 0;
        return 0;
    }
    return AppGateway_Wire(body, out, capacity);
}
