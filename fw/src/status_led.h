#ifndef ET100_STATUS_LED_H
#define ET100_STATUS_LED_H
#include <stdint.h>

struct et100_led_state {
    uint32_t accepted, phase_ms;
    unsigned connected, activity;
};
unsigned et100_led_level(struct et100_led_state *, uint32_t now_ms,
                        unsigned connected, uint32_t accepted);
void et100_status_led_init(void);
#endif
