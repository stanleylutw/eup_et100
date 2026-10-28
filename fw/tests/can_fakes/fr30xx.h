#ifndef ET100_CAN_FAKE_FR30XX_H
#define ET100_CAN_FAKE_FR30XX_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
extern uint32_t fake_can_registers[512];
#define CAN3_BASE ((uintptr_t)fake_can_registers)
#include "driver_can.h"
void fake_can_init_stop(struct_CAN_t *);
#undef __CAN_INIT_STOP
#define __CAN_INIT_STOP(can) fake_can_init_stop(can)
typedef struct {unsigned Pin,Mode,Pull,Alternate;} GPIO_InitTypeDef;
#define GPIO_PIN_10 (1u<<10)
#define GPIO_PIN_11 (1u<<11)
#define GPIO_MODE_AF_PP 1u
#define GPIO_PULLUP 1u
#define GPIO_FUNCTION_5 5u
#define GPIOA ((void *)1)
#define PER_CLK_CANx 1u
#define CAN3_Line0_IRQn 76u
#define __SYSTEM_GPIOA_CLK_ENABLE() ((void)0)
#define __SYSTEM_MCAN3_CLK_ENABLE() ((void)0)
#define __SYSTEM_MCAN_CLK_SELECT_D48M() ((void)0)
#define __SYSTEM_MCAN_CLK_DIV(n) ((void)(n))
void gpio_init(void *,const GPIO_InitTypeDef *);
uint32_t system_get_peripheral_clock(unsigned);
void NVIC_DisableIRQ(unsigned);
void NVIC_EnableIRQ(unsigned);
void NVIC_SetPriority(unsigned,unsigned);
#endif
