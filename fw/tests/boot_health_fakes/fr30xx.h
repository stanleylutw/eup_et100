/* Peripheral surface used by boot_health.c; implemented in test_boot_health.c. */
#include <stdbool.h>
#include <stdint.h>
#define QSPI0 ((void *)0)
#define PMU_REG_WDT_CTRL 0
#define PMU_WDT_EN_BIT 1
void flash_erase(void *qspi, uint32_t offset, uint32_t length);
void flash_write(void *qspi, uint32_t offset, uint32_t length, uint8_t *data);
uint8_t ool_read(uint8_t reg);
void iwdt_Refresh(void);
void NVIC_SystemReset(void);
static inline void __disable_irq(void) {}
static inline void __NOP(void) {}
