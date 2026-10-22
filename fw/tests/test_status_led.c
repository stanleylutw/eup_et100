#include "status_led.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct et100_led_state led = {0};
    assert(et100_led_level(&led, 0, 0, 0));
    assert(et100_led_level(&led, 10000, 0, 0));
    assert(et100_led_level(&led, 10000, 0, 1));
    assert(et100_led_level(&led, 10199, 0, 1));
    assert(!et100_led_level(&led, 10200, 0, 1));
    assert(et100_led_level(&led, 10400, 0, 1));
    assert(!et100_led_level(&led, 12799, 0, 1));
    assert(et100_led_level(&led, 12999, 0, 1));
    assert(et100_led_level(&led, 13000, 0, 1) && !led.activity);
    assert(et100_led_level(&led, 14000, 0, 2));
    assert(!et100_led_level(&led, 14200, 0, 2));
    assert(et100_led_level(&led, 14300, 0, 3));
    assert(et100_led_level(&led, 14400, 1, 3));
    assert(!et100_led_level(&led, 14500, 1, 4));
    assert(et100_led_level(&led, 14600, 1, 4));
    assert(et100_led_level(&led, 14700, 0, 4) && !led.activity);
    assert(et100_led_level(&led, 15000, 0, 4));
    led = (struct et100_led_state){0};
    uint32_t start = UINT32_MAX - 99;
    assert(et100_led_level(&led, start, 1, 0));
    assert(!et100_led_level(&led, start + 100, 1, 0));
    assert(et100_led_level(&led, start + 200, 1, 0));
    led = (struct et100_led_state){0};
    assert(et100_led_level(&led, start, 0, UINT32_MAX));
    assert(!et100_led_level(&led, start + 200, 0, UINT32_MAX));
    assert(et100_led_level(&led, start + 3000, 0, UINT32_MAX));
    assert(et100_led_level(&led, start + 3100, 0, 0) && led.activity);
    puts("PASS: LED idle, filtered activity/retrigger, link priority/disconnect, timer/counter wrap");
    return 0;
}
