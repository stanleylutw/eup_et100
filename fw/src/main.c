/*
 * ET-100 Firmware — minimal skeleton
 *
 * 本檔只是 Phase 0 的佔位符，確立 build 路徑。
 * 實際 bring-up 從 SDK examples/application/ble_simple_periphreal 開始，
 * 待拿到 Freqchip EVB 做首次 build + flash + GPIO blink 驗證後再擴充。
 */

#include "fr30xx.h"

static void delay_ms(uint32_t ms)
{
    /* TODO: 用 SysTick 或 driver_timer 取代忙迴圈 */
    for (volatile uint32_t i = 0; i < ms * 10000; i++) {
        __NOP();
    }
}

int main(void)
{
    /* TODO Phase 1: SystemInit() 在 startup 已呼叫過，這裡做應用初始化
     *   - gpio init (LED pins: Driver / Memory / GPS / Net，見 Sheet 4)
     *   - uart init (debug console on UART4，見 Sheet 2 LTE_DBG)
     *   - i2c init (3 條 bus: EXP_I2C5 / Sensor_I2C0 / NFC_I2C3)
     *   - spi init (SPI_FLASH)
     *   - RTOS kernel start (FreeRTOS，SDK components/modules/FreeRTOS)
     */

    while (1) {
        /* 開機 LED self-test placeholder */
        delay_ms(500);
    }

    /* unreachable */
    return 0;
}
