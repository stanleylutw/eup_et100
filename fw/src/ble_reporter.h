#ifndef ET100_BLE_REPORTER_H
#define ET100_BLE_REPORTER_H
#include <stddef.h>
#include <stdint.h>

#define ET100_REPORT_CACHE 32
#define ET100_REPORT_BATCH 15
struct et100_report_entry {
    uint8_t mac[6], address_type, used, updated, length;
    uint8_t payload[31];
    int8_t rssi;
    uint8_t version, token, reported_version, reported_token, reported;
    uint32_t seen_ms, reported_ms;
};
struct et100_reporter {
    uint16_t cids[4], gtime;
    uint32_t cycle_ms, sent_ms;
    unsigned sent_valid;
    struct et100_report_entry cache[ET100_REPORT_CACHE];
    struct et100_report_entry batch[ET100_REPORT_BATCH];
    unsigned batch_length, batch_index;
};
void et100_reporter_init(struct et100_reporter *, const uint16_t[4], uint16_t, uint32_t);
int et100_reporter_receive(struct et100_reporter *, const uint8_t[6], uint8_t,
                           int8_t, const uint8_t *, size_t, uint32_t);
size_t et100_reporter_poll(struct et100_reporter *, uint32_t, char *, size_t);

/* RTOS adapter: all cache access is serialized with short critical sections. */
void et100_scan_configure(const uint16_t[4], uint16_t, int);
void et100_scan_poll(void (*send)(const char *, size_t, void *), void *);
void et100_scan_start(void);
struct gap_event;
void et100_scan_event(struct gap_event *);
#endif
