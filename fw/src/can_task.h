#ifndef ET100_CAN_TASK_H
#define ET100_CAN_TASK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ET100_CAN_RING_FRAMES 64u
#define ET100_CAN_LINE_MAX 160u
struct et100_can_diagnostics {
    uint32_t received, overflow, rejected, errors, bus_off, tx_dropped;
    unsigned kbps, clock_hz, prescaler, seg1, seg2;
    uint8_t status, locked, tec, rec, passive;
};
/* Task 1 owns the portable gateway/configuration. ISR only copies frames/events.
 * Serial queues commands and consumes complete output lines under its TX lock. */
void et100_can_start(int storage_ready);
size_t et100_can_command(const char *line, char *failure, size_t capacity);
void et100_can_set_paused(bool paused);
size_t et100_can_take_line(char *line, size_t capacity, bool paused);
void et100_can_diagnostics(struct et100_can_diagnostics *out);
/* Portable owner/ISR seams, also exercised without SDK/hardware by host tests. */
void et100_can_step(uint32_t now_ms);
void et100_can_receive_isr(uint32_t id, const uint8_t *data, uint8_t dlc,
                           bool extended, bool remote, bool fd);
void et100_can_error_isr(bool bus_off, bool passive, uint8_t tec, uint8_t rec);
#endif
