/* Source: eup_canbus_32f407/app/app_gateway.h, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    app_gateway.h
 * @brief   V1.9 integer telemetry and S3 mode contract.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details
 * Caller owns the state under app mutex; output callback must copy each line without blocking.
 * - V1.9: CAN task owns mutable state; callbacks copy complete lines without blocking.
 * @history
 * 2026-10-28  Codex  Implement J1939 V1.9 Step 1 (D1-D16).
 * 2026-10-28  Codex  Implement S3 telemetry modes using EVB wire behavior as specification.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
#ifndef APP_GATEWAY_H
#define APP_GATEWAY_H
#include <stdint.h>
#include <stddef.h>
#include "app_can_protocol.h"
typedef enum { APP_GATEWAY_RS232, APP_GATEWAY_J1939, APP_GATEWAY_ASCII } AppGatewayMode_t;
typedef void (*AppGatewayOutput_t)(const char *line, size_t length);
/******************************************************************************
 * @brief   Initialize telemetry state.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details
 * - V1.9: Startup only; initializes unavailable fields, deadlines and default RS232 mode.
 * @input   output: copying callback; now: milliseconds; type4: enable steering bursts.
 * @output  Initialized module state.
 * @algorithm Clear fields/history and report state, install sink/defaults, reset deadline baselines and select RS232 mode.
 * @edge_case Startup is serialized; NULL sink suppresses output and CAN clock strings remain 000000 without a time source.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_Init(AppGatewayOutput_t output, uint32_t now, uint8_t type4);
/******************************************************************************
 * @brief   Configure reporting.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN owner only; resets T2 deadline on GTIME change and requests a fresh state snapshot.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Reset the Type2 deadline only when GTIME changes, copy the configuration and request a Type1 snapshot.
 * @edge_case Caller supplies a validated configuration under app mutex; Type3/4 cadence is not reset by GTIME.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void AppGateway_Configure(const CanConfig_t *config, uint32_t now);
/******************************************************************************
 * @brief   Invalidate a bus epoch.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN owner only; clears field validity and sample history when stopped or changing bitrate.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Clear all field validity and history, reset sample deadline/count and force a future Type1 state report.
 * @edge_case A stop/rate change cannot carry samples into the next bus epoch; raw storage is not a substitute for valid flags.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void AppGateway_Invalidate(uint32_t now);
/******************************************************************************
 * @brief   Request a Type1 snapshot.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Thread-safe nonblocking atomic flag; link task may request on USB open.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Atomically set a pending snapshot flag for the CAN owner to consume on its next Type1 opportunity.
 * @edge_case Repeated requests coalesce; a USB/link caller never mutates field/history caches.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
void AppGateway_RequestSnapshot(void);
/******************************************************************************
 * @brief   Advance S3 output mode.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details CAN task under app mutex; cycle RS232, J1939, ASCII and queue a mode message.
 * - V1.9: CAN owner; mode notices are diagnostic and must be USB-only.
 * @input   None.
 * @output  Selected mode.
 * @algorithm Advance the three-mode enum modulo three, request a snapshot when entering RS232 and enqueue the mode notice.
 * @edge_case Caches and cadence remain intact; transport routes the notice to USB only, keeping UART product output clean.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
AppGatewayMode_t AppGateway_NextMode(void);
/******************************************************************************
 * @brief   Read selected mode.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Read from the owning task or while holding app mutex; no blocking.
 * - V1.9: Owning task or app mutex only; no blocking.
 * @input   None.
 * @output  Selected mode.
 * @algorithm Return the current mode without changing state or deadlines.
 * @edge_case Owner/app-mutex read only; the startup mode is RS232.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
AppGatewayMode_t AppGateway_Mode(void);
/******************************************************************************
 * @brief   Consume a CAN2 extended data frame.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Update known SPNs in every mode, then emit raw J1939 or ASCII lines as selected.
 * - V1.9: CAN owner only; validates field DLC and enumerated invalid raw values, preserving integer precision.
 * @input   id: 29-bit ID; data: DLC bytes; dlc: 0..8; now: milliseconds.
 * @output  Latest field state and mode-dependent lines.
 * @algorithm Validate ID/DLC, derive normalized PGN, optionally emit raw CSV, then extract every mapped field and route ASCII values.
 * @edge_case Bad DLC/enum invalidates that field immediately; unknown PGNs are raw-only and valid all-ones numerical SPNs are retained.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_Frame(uint32_t id, const uint8_t *data, uint8_t dlc, uint32_t now);
/******************************************************************************
 * @brief   Service TTL, sampling and RS232 report deadlines.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details
 * - V1.9: CAN task invokes frequently; Type2 follows configured GTIME, Type3/4 retain 10 s cadence.
 * - V1.9: CAN owner only; at most ten sample slots, missing elapsed seconds become F instead of copied values.
 * @input   now: wraparound-safe milliseconds.
 * @output  Bounded sample/report work; callback must not block.
 * @algorithm Expire fields by TTL, insert missed-second empty samples plus one current sample, then service enabled RS232 change and periodic deadlines.
 * @edge_case At most ten slots are filled after a long pause; unsigned deltas handle wrap and no stale value is copied into missed seconds.
 * @history
 * 2026-10-28  Codex  Initial S3 telemetry implementation.
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16).
 ******************************************************************************/
void AppGateway_Poll(uint32_t now);
/******************************************************************************
 * @brief   Format an official CAN line.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure reentrant XOR formatter; body excludes AT$PD= and CRLF. Rejects truncation.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Check body/output/capacity, XOR the body bytes, and format prefix/body/two-digit XOR/CRLF plus NUL.
 * @edge_case NULL or short output fails with 0 and clears an available buffer; neither prefix nor CRLF participates in XOR.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
size_t AppGateway_Wire(const char *body, char *out, size_t capacity);
/******************************************************************************
 * @brief   Format delayed Type0.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.6
 * @details Pure formatter; caller supplies a consistent configuration and D12 numeric firmware version.
 * @input   Borrowed arguments remain valid for this serialized call.
 * @output  See the return type and behavior above; no ownership transfer.
 * @algorithm Format numeric hardware/version/GTIME/type bits/setting bitrate into a local body and pass it to the reentrant wire formatter.
 * @edge_case NULL settings or a mask above 15 returns 0; output is complete with checksum/CRLF or suppressed if capacity is short.
 * @history
 * 2026-10-28  Codex  Implement V1.9 behavior and failure handling.
 ******************************************************************************/
size_t AppGateway_BootLine(const CanConfig_t *config, unsigned version, char *out, size_t capacity);
#endif
