#ifndef ET100_CAN_FAKE_RTOS_H
#define ET100_CAN_FAKE_RTOS_H
#include <assert.h>
#include <stdint.h>
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdPASS 1
#define pdMS_TO_TICKS(n) (n)
#define portTICK_PERIOD_MS 1
#define configASSERT(n) assert(n)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif
