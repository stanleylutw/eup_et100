/* Source: eup_canbus_32f407/app/app_can_protocol.h, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    app_can_protocol.h
 * @brief   Portable CAN configuration and ASCII command contract.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure data/parser interfaces; no HAL, RTOS, storage or blocking.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#ifndef APP_CAN_PROTOCOL_H
#define APP_CAN_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
typedef struct
{
    uint16_t cartype, gtime;
    uint8_t types, bitrate;
} CanConfig_t;
extern const CanConfig_t CAN_CONFIG_DEFAULT;
typedef enum
{
    CAN_CMD_TYPES,
    CAN_CMD_GTIME,
    CAN_CMD_CARTYPE,
    CAN_CMD_BITRATE,
    CAN_CMD_VERSION,
    CAN_CMD_UNKNOWN
} CanCommand_t;
typedef struct
{
    CanCommand_t command;
    uint8_t setter, valid;
    uint16_t value;
    char name[25], token[5];
} CanRequest_t;
/******************************************************************************
 * @brief   Validate configuration fields.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure bounded checks; Cartype 1 and bitrate 0..2 only.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Check the pointer, Cartype 1, GTIME 1..9999, four-bit type mask and bitrate enum in order.
 * @edge_case NULL configurations and any unsupported field fail without modifying memory.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int CanConfig_Valid(const CanConfig_t *config);
/******************************************************************************
 * @brief   Parse a complete CAN request.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure task/host call; validates optional checksum and token; malformed framing is silent.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Bound and copy printable ASCII, validate optional body XOR, split command/parameter/token, then validate command-specific fields.
 * @edge_case Bad framing/checksum/token returns 0; framed unknown or invalid commands return a request with valid=0 for a failure reply.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int CanProtocol_Parse(const char *line, CanRequest_t *request);
/******************************************************************************
 * @brief   Format a complete CAN reply.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure formatter; echoes source token and per-command failure payload, including D12 version.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Select the success or specified failure payload, format command/value/token, then append body XOR and CRLF through AppGateway_Wire.
 * @edge_case STYPESWITCH failure echoes a valid requested mask; VERSION always includes identity; insufficient capacity returns 0.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
size_t CanProtocol_Reply(const CanRequest_t *request, const CanConfig_t *config, unsigned version,
                         int success, char *out, size_t capacity);
/******************************************************************************
 * @brief   Apply a validated setter to a candidate.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure copy/update; rejects unsupported parameters without changing current configuration.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Validate a setter and current configuration, copy the snapshot, replace the requested field, then validate the whole candidate.
 * @edge_case NULL pointers, getters, unknown commands or invalid values fail; no running configuration or storage is touched.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int CanProtocol_Candidate(const CanRequest_t *request, const CanConfig_t *current, CanConfig_t *next);
#endif
