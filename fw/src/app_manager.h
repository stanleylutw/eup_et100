#ifndef ET100_APP_MANAGER_H
#define ET100_APP_MANAGER_H
#include <stddef.h>
#include <stdint.h>
#include "ble_reporter.h"

#define ET100_MANAGER_DEVICES 8
#define ET100_MANAGER_LIST_MAX 132
#define ET100_MANAGER_SELECTED_MAX 54
struct et100_manager_entry {
    uint8_t used, flags, mac[6], address_type, rssi, adv_type;
    uint8_t identity[6], version, mode, role_state, firmware[3];
    uint8_t raw[31], length;
    uint32_t seen_ms, sequence;
};
struct et100_manager {
    struct et100_manager_entry entries[ET100_MANAGER_DEVICES];
    uint8_t selected;
    uint32_t list_sequence, event_sequence;
};
void et100_manager_init(struct et100_manager *);
void et100_manager_expire(struct et100_manager *, uint32_t);
void et100_manager_update(struct et100_manager *, const uint8_t[6], uint8_t,
    int8_t, uint8_t, const uint8_t *, size_t, const struct et100_adv_details *, uint32_t);
size_t et100_manager_status(struct et100_manager *, uint32_t, uint8_t *, size_t);
size_t et100_manager_list(struct et100_manager *, uint32_t, uint8_t *, size_t);
size_t et100_manager_selected(struct et100_manager *, uint32_t, uint8_t *, size_t);
int et100_manager_select(struct et100_manager *, uint32_t, const uint8_t *, size_t);
#endif
