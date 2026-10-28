/* Source: eup_canbus_32f407/app/app_can_bitrate.h, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    app_can_bitrate.h
 * @brief   Nonblocking 250/500 kbps auto-scan state machine.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure CAN-owner state and apply callback; no delays, RTOS, HAL or I/O ownership assumptions.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#ifndef APP_CAN_BITRATE_H
#define APP_CAN_BITRATE_H
#include <stdint.h>
typedef int (*CanRateApply_t)(void *context, unsigned kbps, int silent);
typedef struct
{
    void *context;
    CanRateApply_t apply;
    uint32_t since, last_valid, last_attempt;
    unsigned candidate;
    uint8_t setting, locked, running, confidence;
} CanBitrate_t;
/******************************************************************************
 * @brief   Initialize scan state.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure state reset; no hardware touched until Set.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Zero the state, install the apply callback/context and select the initial 250 kbps candidate without invoking hardware.
 * @edge_case Caller supplies writable state; a NULL callback remains safe because subsequent transitions report failure.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void CanBitrate_Init(CanBitrate_t *rate, CanRateApply_t apply, void *context);
/******************************************************************************
 * @brief   Select a configured bitrate.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN owner only; apply fixed NORMAL or initial 250k SILENT once; return failure without publishing the new setting.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Reject an unknown setting, map fixed/auto mode to rate and silent flag, apply it, then publish setting and lock status.
 * @edge_case A failed apply does not publish the requested setting; fixed 250/500 modes are immediately locked only after success.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int CanBitrate_Set(CanBitrate_t *rate, uint8_t setting, uint32_t now);
/******************************************************************************
 * @brief   Service scan deadlines.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN owner only; 500ms windows, 5s silence restarts scanning, failed apply retries after 500ms.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Retry failed hardware after 500 ms, leave fixed modes alone, rescan a silent locked bus after 5 s, or alternate passive candidates every 500 ms.
 * @edge_case Unsigned deadline checks survive tick wrap; no-traffic scanning is nonblocking and failed transitions retain retry state.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void CanBitrate_Poll(CanBitrate_t *rate, uint32_t now);
/******************************************************************************
 * @brief   Observe a hardware-validated frame.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN owner only; good=0 resets confidence; three good extended data frames lock NORMAL.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Ignore stopped state, clear confidence on error, update last-valid time for good frames, and enter NORMAL after the third passive good frame.
 * @edge_case Already fixed/locked modes do not accumulate confidence; a failed lock apply leaves hardware stopped for retry.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void CanBitrate_Observe(CanBitrate_t *rate, uint32_t now, int good);
#endif
