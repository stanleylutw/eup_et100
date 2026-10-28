/* Source: eup_canbus_32f407/app/app_demo_j1939_frames.h, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    app_demo_j1939_frames.h
 * @brief   S2 4-phase SAE J1939-71 CAN1 demo frames and expected V1.9 outputs.
 * @author  Antigravity
 * @date    2026-10-28
 * @version v0.2.7
 *
 * @details
 * - Pure data-only header (<stdint.h> and static const tables only); no HAL,
 *   no RTOS dependencies, and no mutable RAM state.
 * - Defines a 4-phase (5 s per phase, 20 s loop) driving scenario across all
 *   14 J1939 PGNs (16 SPNs) used by Protocol V1.9:
 *     Phase 0 (0-5 s)  : Park / Idle   (gear P=251, park brake=1, 0 km/h, 750 rpm)
 *     Phase 1 (5-10 s) : Drive / Accel (gear D1=126, park brake=0, 40 km/h, 1800 rpm, +0.25 rad)
 *     Phase 2 (10-15 s): Cruise        (gear D5=130, park brake=0, 80 km/h, 1500 rpm, +0.0625 rad)
 *     Phase 3 (15-20 s): Brake / Rev   (gear R1=124, park brake=0, 10 km/h, 900 rpm, -0.25 rad)
 * - Every SPN is placed at its exact SAE J1939-71 byte/bit position (1-based,
 *   little-endian); all unused bytes/bits are filled with 0xFF (1s).
 *
 * @history
 * 2026-10-28  Antigravity  Initial draft of 4-phase x 14-PGN J1939 demo table
 ******************************************************************************/
#ifndef APP_DEMO_J1939_FRAMES_H
#define APP_DEMO_J1939_FRAMES_H

#include <stdint.h>

#define DEMO_J1939_PHASES        4U
#define DEMO_J1939_PGNS          14U
#define DEMO_J1939_FRAME_GAP_MS  15U
#define DEMO_J1939_PHASE_MS      5000U

/*
 * 29-bit J1939 Extended IDs: Priority = 6 (0x18), PF/PS = PGN, SA = 0x00.
 * Order matches each PGN index (0..13) in demo_j1939_payload[][14][8].
 */
static const uint32_t demo_j1939_ext_id[DEMO_J1939_PGNS] =
{
    0x18F00400U, /* [0]  PGN 61444 (0xF004) EEC1   : SPN 190 (B4-5), SPN 1675 (B7 bits 1-4) */
    0x18F00500U, /* [1]  PGN 61445 (0xF005) ETC2   : SPN 523 (B4) */
    0x18FEF100U, /* [2]  PGN 65265 (0xFEF1) CCVS1  : SPN 70 (B1 bits 3-4) */
    0x18FEC100U, /* [3]  PGN 65217 (0xFEC1) HRVD   : SPN 917 (B1-4) */
    0x18FEFC00U, /* [4]  PGN 65276 (0xFEFC) DD     : SPN 96 (B2) */
    0x18FEF200U, /* [5]  PGN 65266 (0xFEF2) LFE    : SPN 185 (B5-6) */
    0x18FE6900U, /* [6]  PGN 65129 (0xFE69) ET3    : SPN 1637 (B3-4) */
    0x18FEEE00U, /* [7]  PGN 65262 (0xFEEE) ET1    : SPN 175 (B3-4) */
    0x18FE5600U, /* [8]  PGN 65110 (0xFE56) AT1T1I : SPN 1761 (B1), SPN 3031 (B2) */
    0x18FEE900U, /* [9]  PGN 65257 (0xFEE9) LFC    : SPN 250 (B5-8) */
    0x18FE6C00U, /* [10] PGN 65132 (0xFE6C) TCO1   : SPN 1624 (B7-8) */
    0x18F00300U, /* [11] PGN 61443 (0xF003) EEC2   : SPN 91 (B2) */
    0x18F00100U, /* [12] PGN 61441 (0xF001) EBC1   : SPN 521 (B2) */
    0x18F00900U, /* [13] PGN 61449 (0xF009) VDC2   : SPN 1807 (B1-2) */
};

/*
 * 4 phases x 14 PGNs x 8 bytes.
 *
 * Per-PGN SAE J1939-71 positions and engineering values across phases (P0 / P1 / P2 / P3):
 *   [0]  EEC1   (PGN 61444): SPN 190 B4-5 (0.125 rpm/bit) = 750 / 1800 / 1500 / 900 rpm;
 *                            SPN 1675 B7 bits 1-4 (enum 0-15) = 0 (start not requested) in P0..P3
 *   [1]  ETC2   (PGN 61445): SPN 523 B4 (raw byte) = 251 (P) / 126 (D1) / 130 (D5) / 124 (R1)
 *   [2]  CCVS1  (PGN 65265): SPN 70 B1 bits 3-4 = 1 (set) / 0 (not set) / 0 (not set) / 0 (not set)
 *   [3]  HRVD   (PGN 65217): SPN 917 B1-4 (0.005 km/bit) = 10000.0 / 10000.2 / 10000.8 / 10001.0 km
 *   [4]  DD     (PGN 65276): SPN 96 B2 (0.4 %/bit) = 80.0 / 80.0 / 79.2 / 78.8 %
 *   [5]  LFE    (PGN 65266): SPN 185 B5-6 (1/512 km/L/bit) = 8.50 (4352) / 8.75 (4480) / 9.00 (4608) / 8.75 (4480) km/L
 *   [6]  ET3    (PGN 65129): SPN 1637 B3-4 (0.03125 C/bit, -273) = +85 / +87 / +90 / +89 degC
 *   [7]  ET1    (PGN 65262): SPN 175 B3-4 (0.03125 C/bit, -273) = +95 / +98 / +100 / +99 degC
 *   [8]  AT1T1I (PGN 65110): SPN 1761 B1 (0.4 %/bit) = 70.0 / 70.0 / 69.6 / 69.6 %;
 *                            SPN 3031 B2 (1 C/bit, -40) = +32 / +33 / +34 / +34 degC
 *   [9]  LFC    (PGN 65257): SPN 250 B5-8 (0.5 L/bit) = 1250.0 / 1250.5 / 1251.0 / 1251.5 L
 *   [10] TCO1   (PGN 65132): SPN 1624 B7-8 (1/256 km/h/bit) = 0 / 40 / 80 / 10 km/h
 *   [11] EEC2   (PGN 61443): SPN 91 B2 (0.4 %/bit) = 0 / 40 / 20 / 0 %
 *   [12] EBC1   (PGN 61441): SPN 521 B2 (0.4 %/bit) = 0 / 0 / 0 / 30 %
 *   [13] VDC2   (PGN 61449): SPN 1807 B1-2 (1/1024 rad/bit, -31.374, center 32127) = 0.0 / +0.25 / +0.0625 / -0.25 rad
 */
static const uint8_t demo_j1939_payload[DEMO_J1939_PHASES][DEMO_J1939_PGNS][8] =
{
    /* Phase 0: Park / Idle */
    {
        /* [0]  EEC1   (PGN 61444): SPN 190 B4-5 = 750 rpm (0x1770), SPN 1675 B7 bits 1-4 = 0 (start not requested) */
        { 0xFFU, 0xFFU, 0xFFU, 0x70U, 0x17U, 0xFFU, 0xF0U, 0xFFU },
        /* [1]  ETC2   (PGN 61445): SPN 523 B4 = 251 (0xFB, Park) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFBU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [2]  CCVS1  (PGN 65265): SPN 70 B1 bits 3-4 = 1 (parking brake set) */
        { 0xF7U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [3]  HRVD   (PGN 65217): SPN 917 B1-4 = 10000.000 km (raw 2000000 = 0x001E8480) */
        { 0x80U, 0x84U, 0x1EU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [4]  DD     (PGN 65276): SPN 96 B2 = 80.0 % (raw 200 = 0xC8) */
        { 0xFFU, 0xC8U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [5]  LFE    (PGN 65266): SPN 185 B5-6 = 8.50 km/L (raw 4352 = 0x1100) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U, 0x11U, 0xFFU, 0xFFU },
        /* [6]  ET3    (PGN 65129): SPN 1637 B3-4 = +85 degC (raw 11456 = 0x2CC0) */
        { 0xFFU, 0xFFU, 0xC0U, 0x2CU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [7]  ET1    (PGN 65262): SPN 175 B3-4 = +95 degC (raw 11776 = 0x2E00) */
        { 0xFFU, 0xFFU, 0x00U, 0x2EU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [8]  AT1T1I (PGN 65110): SPN 1761 B1 = 70.0 % (0xAF), SPN 3031 B2 = +32 degC (0x48) */
        { 0xAFU, 0x48U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [9]  LFC    (PGN 65257): SPN 250 B5-8 = 1250.0 L (raw 2500 = 0x000009C4, V1.9 = 12500) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xC4U, 0x09U, 0x00U, 0x00U },
        /* [10] TCO1   (PGN 65132): SPN 1624 B7-8 = 0 km/h (raw 0x0000) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U, 0x00U },
        /* [11] EEC2   (PGN 61443): SPN 91 B2 = 0 % (raw 0x00) */
        { 0xFFU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [12] EBC1   (PGN 61441): SPN 521 B2 = 0 % (raw 0x00) */
        { 0xFFU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [13] VDC2   (PGN 61449): SPN 1807 B1-2 = 0.000 rad (raw 32127 = 0x7D7F -> 10000) */
        { 0x7FU, 0x7DU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
    },

    /* Phase 1: Drive / Accel */
    {
        /* [0]  EEC1   (PGN 61444): SPN 190 B4-5 = 1800 rpm (0x3840), SPN 1675 B7 bits 1-4 = 0 (start not requested) */
        { 0xFFU, 0xFFU, 0xFFU, 0x40U, 0x38U, 0xFFU, 0xF0U, 0xFFU },
        /* [1]  ETC2   (PGN 61445): SPN 523 B4 = 126 (0x7E, D1) */
        { 0xFFU, 0xFFU, 0xFFU, 0x7EU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [2]  CCVS1  (PGN 65265): SPN 70 B1 bits 3-4 = 0 (parking brake not set) */
        { 0xF3U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [3]  HRVD   (PGN 65217): SPN 917 B1-4 = 10000.200 km (raw 2000040 = 0x001E84A8) */
        { 0xA8U, 0x84U, 0x1EU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [4]  DD     (PGN 65276): SPN 96 B2 = 80.0 % (raw 200 = 0xC8) */
        { 0xFFU, 0xC8U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [5]  LFE    (PGN 65266): SPN 185 B5-6 = 8.75 km/L (raw 4480 = 0x1180) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x80U, 0x11U, 0xFFU, 0xFFU },
        /* [6]  ET3    (PGN 65129): SPN 1637 B3-4 = +87 degC (raw 11520 = 0x2D00) */
        { 0xFFU, 0xFFU, 0x00U, 0x2DU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [7]  ET1    (PGN 65262): SPN 175 B3-4 = +98 degC (raw 11872 = 0x2E60) */
        { 0xFFU, 0xFFU, 0x60U, 0x2EU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [8]  AT1T1I (PGN 65110): SPN 1761 B1 = 70.0 % (0xAF), SPN 3031 B2 = +33 degC (0x49) */
        { 0xAFU, 0x49U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [9]  LFC    (PGN 65257): SPN 250 B5-8 = 1250.5 L (raw 2501 = 0x000009C5, V1.9 = 12505) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xC5U, 0x09U, 0x00U, 0x00U },
        /* [10] TCO1   (PGN 65132): SPN 1624 B7-8 = 40 km/h (raw 0x2800) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U, 0x28U },
        /* [11] EEC2   (PGN 61443): SPN 91 B2 = 40 % (raw 100 = 0x64) */
        { 0xFFU, 0x64U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [12] EBC1   (PGN 61441): SPN 521 B2 = 0 % (raw 0x00) */
        { 0xFFU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [13] VDC2   (PGN 61449): SPN 1807 B1-2 = +0.250 rad (raw 32383 = 0x7E7F -> 10100) */
        { 0x7FU, 0x7EU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
    },

    /* Phase 2: Cruise */
    {
        /* [0]  EEC1   (PGN 61444): SPN 190 B4-5 = 1500 rpm (0x2EE0), SPN 1675 B7 bits 1-4 = 0 (start not requested) */
        { 0xFFU, 0xFFU, 0xFFU, 0xE0U, 0x2EU, 0xFFU, 0xF0U, 0xFFU },
        /* [1]  ETC2   (PGN 61445): SPN 523 B4 = 130 (0x82, D5) */
        { 0xFFU, 0xFFU, 0xFFU, 0x82U, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [2]  CCVS1  (PGN 65265): SPN 70 B1 bits 3-4 = 0 (parking brake not set) */
        { 0xF3U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [3]  HRVD   (PGN 65217): SPN 917 B1-4 = 10000.800 km (raw 2000160 = 0x001E8520) */
        { 0x20U, 0x85U, 0x1EU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [4]  DD     (PGN 65276): SPN 96 B2 = 79.2 % (raw 198 = 0xC6, rounds to 79) */
        { 0xFFU, 0xC6U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [5]  LFE    (PGN 65266): SPN 185 B5-6 = 9.00 km/L (raw 4608 = 0x1200) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U, 0x12U, 0xFFU, 0xFFU },
        /* [6]  ET3    (PGN 65129): SPN 1637 B3-4 = +90 degC (raw 11616 = 0x2D60) */
        { 0xFFU, 0xFFU, 0x60U, 0x2DU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [7]  ET1    (PGN 65262): SPN 175 B3-4 = +100 degC (raw 11936 = 0x2EA0) */
        { 0xFFU, 0xFFU, 0xA0U, 0x2EU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [8]  AT1T1I (PGN 65110): SPN 1761 B1 = 69.6 % (0xAE, rounds to 70), SPN 3031 B2 = +34 degC (0x4A) */
        { 0xAEU, 0x4AU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [9]  LFC    (PGN 65257): SPN 250 B5-8 = 1251.0 L (raw 2502 = 0x000009C6, V1.9 = 12510) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xC6U, 0x09U, 0x00U, 0x00U },
        /* [10] TCO1   (PGN 65132): SPN 1624 B7-8 = 80 km/h (raw 0x5000) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U, 0x50U },
        /* [11] EEC2   (PGN 61443): SPN 91 B2 = 20 % (raw 50 = 0x32) */
        { 0xFFU, 0x32U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [12] EBC1   (PGN 61441): SPN 521 B2 = 0 % (raw 0x00) */
        { 0xFFU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [13] VDC2   (PGN 61449): SPN 1807 B1-2 = +0.0625 rad (raw 32191 = 0x7DBF -> 10040) */
        { 0xBFU, 0x7DU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
    },

    /* Phase 3: Brake / Reverse */
    {
        /* [0]  EEC1   (PGN 61444): SPN 190 B4-5 = 900 rpm (0x1C20), SPN 1675 B7 bits 1-4 = 0 (start not requested) */
        { 0xFFU, 0xFFU, 0xFFU, 0x20U, 0x1CU, 0xFFU, 0xF0U, 0xFFU },
        /* [1]  ETC2   (PGN 61445): SPN 523 B4 = 124 (0x7C, R1) */
        { 0xFFU, 0xFFU, 0xFFU, 0x7CU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [2]  CCVS1  (PGN 65265): SPN 70 B1 bits 3-4 = 0 (parking brake not set) */
        { 0xF3U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [3]  HRVD   (PGN 65217): SPN 917 B1-4 = 10001.000 km (raw 2000200 = 0x001E8548) */
        { 0x48U, 0x85U, 0x1EU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [4]  DD     (PGN 65276): SPN 96 B2 = 78.8 % (raw 197 = 0xC5, rounds to 79) */
        { 0xFFU, 0xC5U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [5]  LFE    (PGN 65266): SPN 185 B5-6 = 8.75 km/L (raw 4480 = 0x1180) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x80U, 0x11U, 0xFFU, 0xFFU },
        /* [6]  ET3    (PGN 65129): SPN 1637 B3-4 = +89 degC (raw 11584 = 0x2D40) */
        { 0xFFU, 0xFFU, 0x40U, 0x2DU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [7]  ET1    (PGN 65262): SPN 175 B3-4 = +99 degC (raw 11904 = 0x2E80) */
        { 0xFFU, 0xFFU, 0x80U, 0x2EU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [8]  AT1T1I (PGN 65110): SPN 1761 B1 = 69.6 % (0xAE, rounds to 70), SPN 3031 B2 = +34 degC (0x4A) */
        { 0xAEU, 0x4AU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [9]  LFC    (PGN 65257): SPN 250 B5-8 = 1251.5 L (raw 2503 = 0x000009C7, V1.9 = 12515) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xC7U, 0x09U, 0x00U, 0x00U },
        /* [10] TCO1   (PGN 65132): SPN 1624 B7-8 = 10 km/h (raw 0x0A00) */
        { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U, 0x0AU },
        /* [11] EEC2   (PGN 61443): SPN 91 B2 = 0 % (raw 0x00) */
        { 0xFFU, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [12] EBC1   (PGN 61441): SPN 521 B2 = 30 % (raw 75 = 0x4B) */
        { 0xFFU, 0x4BU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
        /* [13] VDC2   (PGN 61449): SPN 1807 B1-2 = -0.250 rad (raw 31871 = 0x7C7F -> 00100) */
        { 0x7FU, 0x7CU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
    },
};

/* Expected Protocol V1.9 Type 1 lines (without CRLF) for each phase transition. */
static const char demo_j1939_expected_t1[DEMO_J1939_PHASES][29] =
{
    "AT$PD=67;1;000000;0;251;1*3C",
    "AT$PD=67;1;000000;0;126;0*3E",
    "AT$PD=67;1;000000;0;130;0*39",
    "AT$PD=67;1;000000;0;124;0*3C",
};

/* Expected Protocol V1.9 Type 2 lines (without CRLF) when sampled in each phase. */
static const char demo_j1939_expected_t2[DEMO_J1939_PHASES][57] =
{
    "AT$PD=67;2;000000;10000;80;4352;185;195;70:132;12500*0B",
    "AT$PD=67;2;000000;10000;80;4480;187;198;70:133;12505*08",
    "AT$PD=67;2;000000;10000;79;4608;190;1100;70:134;12510*39",
    "AT$PD=67;2;000000;10001;79;4480;189;199;70:134;12515*06",
};

/* Expected Protocol V1.9 Type 3 one-second sample (10 hex chars + NUL) per phase. */
static const char demo_j1939_expected_t3[DEMO_J1939_PHASES][11] =
{
    "0002EE0000",
    "2807082800",
    "5005DC1400",
    "0A0384001E",
};

/* Expected Protocol V1.9 Type 4 one-second sample (5 chars + NUL) per phase. */
static const char demo_j1939_expected_t4[DEMO_J1939_PHASES][6] =
{
    "10000",
    "10100",
    "10040",
    "00100",
};

#endif /* APP_DEMO_J1939_FRAMES_H */
