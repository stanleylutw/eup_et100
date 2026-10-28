/* Source: eup_canbus_32f407/app/j1939.h, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    j1939.h
 * @brief   SAE J1939-21 29-bit identifier decoding (pure functions, no HW).
 * @author  Claude Code
 * @date    2026-10-27
 * @version v0.1.4
 *
 * @details
 * - ID layout: P[28:26] EDP[25] DP[24] PF[23:16] PS[15:8] SA[7:0].
 * - PDU1 (PF < 240): PS is the destination address, PGN low byte = 0.
 * - PDU2 (PF >= 240): PS is the group extension, PGN includes PS, DA = 0xFF.
 *
 * @history
 * 2026-10-27  Claude Code  Initial version (bring-up stage 5)
 ******************************************************************************/
#ifndef J1939_H
#define J1939_H

#include <stdint.h>

#define J1939_ADDR_GLOBAL   0xFFU
#define J1939_PF_PDU2_MIN   240U

typedef struct
{
    uint8_t  priority;   /* 0 (highest) .. 7 */
    uint32_t pgn;        /* 18-bit parameter group number */
    uint8_t  sa;         /* source address */
    uint8_t  da;         /* destination address (0xFF for PDU2 / broadcast) */
    uint8_t  pdu2;       /* 1 = PDU2 (broadcast) format */
} J1939Id_t;

/**
 * @brief Split a 29-bit CAN identifier into J1939 fields.
 * @input can_id: 29-bit extended identifier
 * @output out: priority, PGN, SA, DA, PDU format
 * @algorithm PF < 240 -> PDU1 (DA = PS, PGN = EDP|DP|PF|00), else PDU2 (PGN = EDP|DP|PF|PS)
 * @edge_case Bits above 28 are ignored.
 */
void J1939_DecodeId(uint32_t can_id, J1939Id_t *out);

/**
 * @brief Short name for a few well-known PGNs (for console output).
 * @input pgn
 * @output constant string, "" when unknown
 */
const char *J1939_PgnName(uint32_t pgn);

#endif /* J1939_H */
