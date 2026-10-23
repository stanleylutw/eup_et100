#ifndef ET100_SYSTEM_STATUS_H
#define ET100_SYSTEM_STATUS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SYS_COUNT 33u
#define SYS_LINE_MAX 320u
enum sys_index { SYS_SYSTEM=1, SYS_SETTINGS=2, SYS_NOR=3, SYS_BLE=4,
    SYS_RS232_0=13, SYS_WATCHDOG=29, SYS_RESET=30 };
enum sys_status { SYS_UNKNOWN=0, SYS_DISABLED=240, SYS_NOT_FITTED=241,
    SYS_UNSUPPORTED=242, SYS_STALE=243, SYS_ERROR=244 };
enum sys_ble_role { SYS_BLE_SCAN=1, SYS_BLE_ADV=2, SYS_BLE_LINK=4 };

void et100_sys_init(void);
bool et100_sys_valid(unsigned index, unsigned status);
void et100_sys_set(unsigned index, unsigned status);
void et100_sys_snapshot(uint8_t out[SYS_COUNT]);
void et100_sys_ble(unsigned role, bool active);
/* Called before the first notification only; unproven reset causes stay 0. */
void et100_sys_boot_trial(bool trial);
size_t et100_sys_query(const char *token, unsigned error, char *out, size_t capacity);
/* Serial task only: coalesced, bounded, at most one line per 500 ms. */
size_t et100_sys_poll(uint32_t now_ms, bool paused, char *out, size_t capacity);
#endif
