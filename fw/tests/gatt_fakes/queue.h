#include "FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned length, unsigned size);
BaseType_t xQueueSend(QueueHandle_t, const void *, uint32_t);
BaseType_t xQueueReceive(QueueHandle_t, void *, uint32_t);
unsigned uxQueueMessagesWaiting(QueueHandle_t);
