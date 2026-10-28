/* Source: eup_canbus_32f407/app/j1939.c, commit a730897; unchanged implementation. */
/******************************************************************************
 * @file    j1939.c
 * @brief   SAE J1939-21 29-bit identifier decoding (pure functions, no HW).
 * @author  Claude Code
 * @date    2026-10-27
 * @version v0.1.4
 *
 * @details
 * - No dependency on HAL or BSP, so it can be unit-tested on the host.
 * - PGN name table is a console aid only; SPN decoding belongs to stage 6.
 *
 * @history
 * 2026-10-27  Claude Code  Initial version (bring-up stage 5)
 ******************************************************************************/
#include <stddef.h>
#include "j1939.h"

typedef struct
{
    uint32_t    pgn;
    const char *name;
} J1939PgnName_t;

/* PGNs used by the EVB Type1~4 outputs plus common transport / request. */
static const J1939PgnName_t j1939_pgn_names[] =
{
    { 0xEA00U, "REQUEST" },     /* 59904 request PGN */
    { 0xEB00U, "TP.DT" },       /* 60160 transport data */
    { 0xEC00U, "TP.CM" },       /* 60416 transport connection mgmt */
    { 0xEE00U, "ADDR_CLAIM" },  /* 60928 address claimed */
    { 0xF003U, "EEC2" },        /* 61443 accel pedal, engine load */
    { 0xF004U, "EEC1" },        /* 61444 engine speed */
    { 0xFEC1U, "VDHR" },        /* 65217 high-res vehicle distance */
    { 0xFEE5U, "HOURS" },       /* 65253 engine hours */
    { 0xFEE9U, "LFC" },         /* 65257 fuel consumption */
    { 0xFEEEU, "ET1" },         /* 65262 engine temperature */
    { 0xFEEFU, "EFL/P1" },      /* 65263 engine fluid level / pressure */
    { 0xFEF1U, "CCVS" },        /* 65265 vehicle speed, brake, clutch */
    { 0xFEF2U, "LFE" },         /* 65266 fuel economy */
    { 0xFEF5U, "AMB" },         /* 65269 ambient conditions */
    { 0xFEFCU, "DD" },          /* 65276 dash display (fuel level) */
    { 0xF005U, "ETC2" },        /* 61445 transmission gear */
    { 0xFECAU, "DM1" },         /* 65226 active diagnostic codes */
};

/******************************************************************************
 * @brief   Split a 29-bit CAN identifier into J1939 fields.
 * @author  Claude Code
 * @date    2026-10-27
 * @version v0.1.4
 *
 * @details
 * - Step 1: extract raw bit fields.
 * - Step 2: PDU1 vs PDU2 decides whether PS is DA or part of the PGN.
 *
 * @history
 * 2026-10-27  Claude Code  Initial version
 ******************************************************************************/
void J1939_DecodeId(uint32_t can_id, J1939Id_t *out)
{
    uint32_t edp_dp;
    uint8_t  pf;
    uint8_t  ps;

    if (out == NULL)
    {
        return;
    }

    /* Step 1: raw fields. */
    out->priority = (uint8_t)((can_id >> 26) & 0x7U);
    edp_dp        = (can_id >> 24) & 0x3U;           /* EDP:DP, 2 bits */
    pf            = (uint8_t)((can_id >> 16) & 0xFFU);
    ps            = (uint8_t)((can_id >> 8) & 0xFFU);
    out->sa       = (uint8_t)(can_id & 0xFFU);

    /* Step 2: PDU format. */
    if (pf < J1939_PF_PDU2_MIN)
    {
        out->pdu2 = 0U;
        out->da   = ps;                               /* destination specific */
        out->pgn  = (edp_dp << 16) | ((uint32_t)pf << 8);
    }
    else
    {
        out->pdu2 = 1U;
        out->da   = J1939_ADDR_GLOBAL;                /* broadcast */
        out->pgn  = (edp_dp << 16) | ((uint32_t)pf << 8) | ps;
    }
}

/******************************************************************************
 * @brief   Short name for a few well-known PGNs.
 * @author  Claude Code
 * @date    2026-10-27
 * @version v0.1.4
 *
 * @details
 * - Linear search over a small table; console use only.
 *
 * @history
 * 2026-10-27  Claude Code  Initial version
 ******************************************************************************/
const char *J1939_PgnName(uint32_t pgn)
{
    uint32_t i;

    for (i = 0U; i < (sizeof(j1939_pgn_names) / sizeof(j1939_pgn_names[0])); i++)
    {
        if (j1939_pgn_names[i].pgn == pgn)
        {
            return j1939_pgn_names[i].name;
        }
    }
    return "";
}
