/* Source: eup_canbus_32f407/app/app_can_bitrate.c, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    app_can_bitrate.c
 * @brief   Bounded bitrate detection and silence recovery.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN owner calls against millisecond time; apply callback may use bounded HAL initialization but never sleeps for a scan window.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#include "app_can_bitrate.h"
#include <string.h>
/******************************************************************************
 * @brief   CanBitrate_Init — Bounded bitrate detection and silence recovery.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN-owner initialization resets RAM state and installs callbacks only; no HAL call, allocation or waiting.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Zero the state, install the apply callback/context and select the initial 250 kbps candidate without invoking hardware.
 * @edge_case Caller supplies writable state; a NULL callback remains safe because subsequent transitions report failure.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void CanBitrate_Init(CanBitrate_t *rate, CanRateApply_t apply, void *context)
{
    memset(rate, 0, sizeof(*rate));
    rate->apply = apply;
    rate->context = context;
    rate->candidate = 250U;
}
/******************************************************************************
 * @brief   change — Bounded bitrate detection and silence recovery.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN owner apply callback configures actual hardware. Failed apply marks running false; successful switch resets confidence and observation deadlines.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Record attempt time, invoke hardware apply, then publish candidate/running state and reset observation deadlines/confidence on success.
 * @edge_case Missing or failed callback marks running false and retains the prior candidate for retry; no scan-window sleep occurs.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
static int change(CanBitrate_t *r, unsigned kbps, int silent, uint32_t now)
{
    r->last_attempt = now;
    if (!r->apply || !r->apply(r->context, kbps, silent))
    {
        r->running = 0U;
        return 0;
    }
    r->candidate = kbps;
    r->running = 1U;
    r->since = now;
    r->last_valid = now;
    r->confidence = 0U;
    return 1;
}
/******************************************************************************
 * @brief   CanBitrate_Set — Bounded bitrate detection and silence recovery.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN owner calls against millisecond time; apply callback may use bounded HAL initialization but never sleeps for a scan window.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Reject an unknown setting, map fixed/auto mode to rate and silent flag, apply it, then publish setting and lock status.
 * @edge_case A failed apply does not publish the requested setting; fixed 250/500 modes are immediately locked only after success.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
int CanBitrate_Set(CanBitrate_t *r, uint8_t setting, uint32_t now)
{
    if (setting > 2U)
    {
        return 0;
    }
    unsigned kbps = setting == 2U ? 500U : 250U;
    if (!change(r, kbps, setting == 0U, now))
    {
        return 0;
    }
    r->setting = setting;
    r->locked = setting != 0U;
    return 1;
}
/******************************************************************************
 * @brief   CanBitrate_Poll — Bounded bitrate detection and silence recovery.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure nonblocking deadline transitions; fixed modes never switch themselves, failed hardware only retries at 500ms intervals.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Retry failed hardware after 500 ms, leave fixed modes alone, rescan a silent locked bus after 5 s, or alternate passive candidates every 500 ms.
 * @edge_case Unsigned deadline checks survive tick wrap; no-traffic scanning is nonblocking and failed transitions retain retry state.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void CanBitrate_Poll(CanBitrate_t *r, uint32_t now)
{
    if (!r->running)
    {
        if ((uint32_t)(now - r->last_attempt) >= 500U)
        {
            (void)CanBitrate_Set(r, r->setting, now);
        }
        return;
    }
    if (r->setting)
    {
        return;
    }
    if (r->locked)
    {
        if ((uint32_t)(now - r->last_valid) >= 5000U)
        {
            if (change(r, 250U, 1, now))
            {
                r->locked = 0U;
            }
        }
    }
    else if ((uint32_t)(now - r->since) >= 500U)
    {
        (void)change(r, r->candidate == 250U ? 500U : 250U, 1, now);
    }
}
/******************************************************************************
 * @brief   CanBitrate_Observe — Bounded bitrate detection and silence recovery.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Only CRC-valid 29-bit data frames count; caller reports new hardware error/overrun as bad, resetting the current streak.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Ignore stopped state, clear confidence on error, update last-valid time for good frames, and enter NORMAL after the third passive good frame.
 * @edge_case Already fixed/locked modes do not accumulate confidence; a failed lock apply leaves hardware stopped for retry.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void CanBitrate_Observe(CanBitrate_t *r, uint32_t now, int good)
{
    if (!r->running)
    {
        return;
    }
    if (!good)
    {
        r->confidence = 0U;
        return;
    }
    r->last_valid = now;
    if (r->setting || r->locked)
    {
        return;
    }
    if (++r->confidence >= 3U)
    {
        if (change(r, r->candidate, 0, now))
        {
            r->locked = 1U;
        }
    }
}
