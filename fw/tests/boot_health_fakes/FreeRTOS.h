#ifndef ET100_FAKE_FREERTOS_H
#define ET100_FAKE_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(n) ((TickType_t)(n))
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif
