/* Source: eup_canbus_32f407/tests/host/test_demo_frames.c, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    test_demo_frames.c
 * @brief   Verify the immutable drive-demo expectations through the real gateway.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.7
 * @details Host-only ASan/UBSan test; all IDs, payloads and expected values come
 * from app_demo_j1939_frames.h. A mismatch fails without altering either source.
 * @history
 * 2026-10-28  Codex  Add four-phase V1.9 Type1-4 conformance gate.
 ******************************************************************************/
#include "app_demo_j1939_frames.h"
#include "app_gateway.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char reports[5][160];
static unsigned counts[5];

/******************************************************************************
 * @brief   Capture one complete report and independently verify its checksum.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.7
 * @details Called synchronously by the real gateway; no device or blocking I/O.
 * @input   line and length: complete borrowed report including CRLF.
 * @output  One bounded copy and occurrence count per Type1-4.
 * @algorithm Check frame syntax/length, XOR body bytes, then copy by type number.
 * @edge_case Duplicate types, truncated lines or bad XOR abort this host test.
 * @history
 * 2026-10-28  Codex  Validate complete wire framing alongside immutable examples.
 ******************************************************************************/
static void capture(const char *line, size_t length)
{
    assert(length < sizeof(reports[0]) && length >= 16U);
    assert(!strncmp(line, "AT$PD=67;", 9U));
    assert(line[9] >= '1' && line[9] <= '4' && line[10] == ';');
    assert(line[length - 2U] == '\r' && line[length - 1U] == '\n');
    const char *star = memchr(line, '*', length);
    assert(star && (size_t)(star - line) == length - 5U);
    unsigned checksum = 0U;
    for (const char *p = line + 6U; p != star; p++)
    {
        checksum ^= (unsigned char)*p;
    }
    char encoded[3];
    (void)snprintf(encoded, sizeof(encoded), "%02X", checksum);
    assert(!memcmp(star + 1U, encoded, 2U));
    unsigned type = (unsigned)(line[9] - '0');
    assert(++counts[type] == 1U);
    memcpy(reports[type], line, length);
    reports[type][length] = '\0';
}

/******************************************************************************
 * @brief   Compare a complete Type1/2 line with the header's exact expected wire.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.7
 * @details Host assertion gate; emits both values on a mismatch for STOP/report.
 * @input   phase/type indexes and immutable expected line without CRLF.
 * @output  Failure exits nonzero; matching bytes permit the next conformance check.
 * @algorithm Append CRLF in local scratch and compare the captured complete line.
 * @edge_case The data header and gateway are read-only inputs, even on failure.
 * @history
 * 2026-10-28  Codex  Check per-phase complete Type1/2 strings byte for byte.
 ******************************************************************************/
static void expect_line(unsigned phase, unsigned type, const char *expected)
{
    char wire[160];
    int n = snprintf(wire, sizeof(wire), "%s\r\n", expected);
    assert(n > 0 && (size_t)n < sizeof(wire));
    if (strcmp(reports[type], wire))
    {
        fprintf(stderr, "STOP phase=%u type=%u\nexpected: %sactual:   %s", phase, type, wire,
                reports[type]);
        exit(1);
    }
}

/******************************************************************************
 * @brief   Compare the Type3/4 one-second sample exactly with header expectations.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.7
 * @details Host only; capture has already verified the complete line's XOR/CRLF.
 * @input   phase/type indexes and one immutable expected packed sample.
 * @output  Nonzero process exit on any differing payload byte or sample count.
 * @algorithm Require the one-sample prefix, compare sample bytes, then require '*'.
 * @edge_case Prevents a matching substring in an extra, stale or multi-sample payload.
 * @history
 * 2026-10-28  Codex  Check all four phases' exact Type3/4 samples.
 ******************************************************************************/
static void expect_sample(unsigned phase, unsigned type, const char *expected)
{
    char prefix[32];
    int n = snprintf(prefix, sizeof(prefix), "AT$PD=67;%u;000000;1;", type);
    assert(n > 0 && (size_t)n < sizeof(prefix));
    size_t sample_length = strlen(expected);
    if (strncmp(reports[type], prefix, (size_t)n) ||
        strncmp(reports[type] + n, expected, sample_length) || reports[type][n + sample_length] != '*')
    {
        fprintf(stderr, "STOP phase=%u type=%u sample expected=%s actual=%s", phase, type, expected,
                reports[type]);
        exit(1);
    }
}

/******************************************************************************
 * @brief   Decode all 56 immutable payloads and verify all sixteen expected reports.
 * @author  Codex
 * @date    2026-10-28
 * @version v0.2.7
 * @details Runs the production gateway in a fresh epoch for each phase; no BSP fake.
 * @input   None; all source and expected data reside in the immutable header.
 * @output  PASS only after four complete Type1/2 lines and four Type3/4 samples match.
 * @algorithm Configure defaults, start a fresh one-sample epoch at the 30s report
 * deadline, feed each phase's 14 frames, poll once and compare all four reports.
 * @edge_case Feeding at the report timestamp keeps short-TTL fields fresh; the
 * explicit epoch reset avoids manufacturing ten seconds of historical samples.
 * @history
 * 2026-10-28  Codex  Gate the S2 integration on exact existing-gateway compatibility.
 ******************************************************************************/
int main(void)
{
    for (unsigned phase = 0U; phase < DEMO_J1939_PHASES; phase++)
    {
        memset(reports, 0, sizeof(reports));
        memset(counts, 0, sizeof(counts));
        AppGateway_Init(capture, 0U, 1U);
        const CanConfig_t config = {1U, 30U, 15U, 0U};
        AppGateway_Configure(&config, 0U);
        AppGateway_Invalidate(30000U);
        for (unsigned frame = 0U; frame < DEMO_J1939_PGNS; frame++)
        {
            AppGateway_Frame(demo_j1939_ext_id[frame], demo_j1939_payload[phase][frame], 8U, 30000U);
        }
        AppGateway_Poll(30000U);
        expect_line(phase, 1U, demo_j1939_expected_t1[phase]);
        expect_line(phase, 2U, demo_j1939_expected_t2[phase]);
        expect_sample(phase, 3U, demo_j1939_expected_t3[phase]);
        expect_sample(phase, 4U, demo_j1939_expected_t4[phase]);
        printf("PASS demo phase %u: exact Type1/2 wires and Type3/4 samples\n", phase);
    }
    puts("PASS immutable demo: 56 payloads through production gateway, all Type1-4 expectations");
    return 0;
}
