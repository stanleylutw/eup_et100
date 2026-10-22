#include "FreeRTOS.h"
uint32_t xTaskGetTickCount(void);
void vTaskDelay(uint32_t ticks);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
BaseType_t xTaskCreate(void (*task)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
