#include "status_led.h"

unsigned et100_led_level(struct et100_led_state *led, uint32_t now,
                        unsigned connected, uint32_t accepted)
{
    if (connected) {
        if (!led->connected) led->phase_ms = now;
        led->connected = 1;
        led->activity = 0;
        led->accepted = accepted;
        return ((uint32_t)(now - led->phase_ms) / 100) % 2 == 0;
    }
    if (led->connected) {
        led->connected = 0;
        led->activity = 0;
        led->accepted = accepted;
    } else if (led->accepted != accepted) {
        led->accepted = accepted;
        led->phase_ms = now;
        led->activity = 1;
    }
    uint32_t elapsed = now - led->phase_ms;
    if (led->activity && elapsed < 3000) return (elapsed / 200) % 2 == 0;
    led->activity = 0;
    return 1;
}

#ifndef ET100_LED_HOST_TEST
#include "fr30xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gatt_client.h"

extern volatile unsigned et100_scan_accepted;

static void led_task(void *argument)
{
    (void)argument;
    struct et100_led_state led = {0};
    for (;;) {
        unsigned on = et100_led_level(&led,
            (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS,
            et100_gatt_link_connected(), et100_scan_accepted);
        gpio_write_pin(GPIOA, GPIO_PIN_7, on ? GPIO_PIN_SET : GPIO_PIN_CLEAR);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void et100_status_led_init(void)
{
    __SYSTEM_GPIOA_CLK_ENABLE();
    /* Preload low before enabling outputs: D3 remains unused/off. */
    gpio_write_pin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_CLEAR);
    GPIO_InitTypeDef pins = {0};
    pins.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    pins.Mode = GPIO_MODE_OUTPUT_PP;
    pins.Pull = GPIO_NOPULL;
    gpio_init(GPIOA, &pins);
    BaseType_t created = xTaskCreate(led_task, "led", 384, NULL, 1, NULL);
    configASSERT(created == pdPASS);
}
#endif
