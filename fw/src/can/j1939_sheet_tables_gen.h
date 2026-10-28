/* Source: eup_canbus_32f407/app/j1939_sheet_tables_gen.h, commit a730897; unchanged implementation. */
/* @file j1939_sheet_tables_gen.h
 * @brief Generated V1.9 rules; DO NOT EDIT.
 * @author Codex
 * @date 2026-10-28
 * @version v0.2.6
 * @details Generated offline from docs/02_protocol/j1939_v19_rules.csv.
 * @history 2026-10-28 Codex Generate S3 rules without modifying reference data.
 * 2026-10-28  Codex  Implement exact D1-D16 integer rules.
 * 2026-10-28  Codex  J1939 V1.9 Step 1 (D1-D16). */
#ifndef J1939_SHEET_TABLES_GEN_H
#define J1939_SHEET_TABLES_GEN_H
#include <stdint.h>
typedef enum { FIELD_GEAR, FIELD_HANDBRAKE, FIELD_MILEAGE, FIELD_FUEL, FIELD_ECONOMY, FIELD_WATER, FIELD_OIL, FIELD_UREA, FIELD_UREA_TEMP, FIELD_TOTAL_FUEL, FIELD_SPEED, FIELD_RPM, FIELD_THROTTLE, FIELD_BRAKE, FIELD_STEER, FIELD_STARTER, FIELD_COUNT } GatewayField_t;
typedef struct { uint32_t pgn, spn, ttl_ms; int32_t numerator, denominator, offset;
    uint8_t byte, bytes, bit, bits, type; uint32_t invalid_mask; } GatewayRule_t;
static const GatewayRule_t gateway_rules[FIELD_COUNT] = {
    {61445U,523U,3000U,1,1,0,3U,1U,0U,0U,1U,0x00000000U},
    {65265U,70U,3000U,1,1,0,0U,1U,2U,2U,1U,0x0000000CU},
    {65217U,917U,90000U,5,1000,0,0U,4U,0U,0U,2U,0x00000000U},
    {65276U,96U,90000U,2,5,0,1U,1U,0U,0U,2U,0x00000000U},
    {65266U,185U,90000U,1,512,0,4U,2U,0U,0U,2U,0x00000000U},
    {65129U,1637U,90000U,1,32,-8736,2U,2U,0U,0U,2U,0x00000000U},
    {65262U,175U,90000U,1,32,-8736,2U,2U,0U,0U,2U,0x00000000U},
    {65110U,1761U,90000U,2,5,0,0U,1U,0U,0U,2U,0x00000000U},
    {65110U,3031U,90000U,1,1,-40,1U,1U,0U,0U,2U,0x00000000U},
    {65257U,250U,90000U,1,2,0,4U,4U,0U,0U,2U,0x00000000U},
    {65132U,1624U,3000U,1,256,0,6U,2U,0U,0U,3U,0x00000000U},
    {61444U,190U,3000U,1,8,0,3U,2U,0U,0U,3U,0x00000000U},
    {61443U,91U,3000U,2,5,0,1U,1U,0U,0U,3U,0x00000000U},
    {61441U,521U,3000U,2,5,0,1U,1U,0U,0U,3U,0x00000000U},
    {61449U,1807U,3000U,1,1024,-32127,0U,2U,0U,0U,4U,0x00000000U},
    {61444U,1675U,3000U,1,1,0,6U,1U,0U,4U,1U,0x00008000U},
};
#define GATEWAY_RULE_COUNT FIELD_COUNT
#endif
