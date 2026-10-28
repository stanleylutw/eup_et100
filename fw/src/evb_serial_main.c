/* DEV1.4 serial target, derived from the patched vendor BLE example main.c.
 * UART3 is dedicated to protocol frames; vendor printf output is discarded.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "fr30xx.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "fdb_app.h"
#include "host.h"

#include "app_task.h"
#include "app_at.h"
#include "serial_gchk.h"
#include "status_led.h"
#include "system_status.h"
#include "can_task.h"

/* hardware handlers */
static UART_HandleTypeDef Uart3_handle;
static CALI_HandleTypeDef cali_handle;

#if ENABLE_RTOS_MONITOR == 1
/* FreeRTOS running status monitor task */
static TaskHandle_t monitor_task_handle;
volatile unsigned int CPU_RunTime;
static uint8_t CPU_RunInfo[2048];
#endif

/* APP task */
TaskHandle_t app_task_handle;

#ifdef ENABLE_RTTHREAD
static uint32_t rt_heap[configTOTAL_HEAP_SIZE/sizeof(uint32_t)];
int ulTimerCountsForOneTick = 96000;
int xMaximumPossibleSuppressedTicks = 10000;
int ulStoppedTimerCompensation = 45;
#endif

void controller_start(void);
void host_start(void);

#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
int fputc(int c, FILE *fp)
{
    uart_transmit(&Uart3_handle, (void *)&c, 1);
    while(!(Uart3_handle.UARTx->USR.TFE));

    return c;
}
#endif
#ifdef __GNUC__
int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    return len;
}
#endif

#ifdef __ICCARM__
int putchar(int c)
{
    uart_transmit(&Uart3_handle, (void *)&c, 1);
    while(!(Uart3_handle.UARTx->USR.TFE));
    return c;
}
#endif

#if ENABLE_RTOS_MONITOR == 1
static void monitor_task(void *arg)
{
    while(1) {
        vTaskDelay(2000000);

        memset(CPU_RunInfo,0,2048);
        vTaskList((char *)&CPU_RunInfo);
        printf("---------------------------------------------\r\n");
        printf("name           	state     priority  stack     seq\r\n");
        printf("%s", CPU_RunInfo);
        printf("---------------------------------------------\r\n");

        memset(CPU_RunInfo,0,400);
        vTaskGetRunTimeStats((char *)&CPU_RunInfo);
        printf("name                counter             usage\r\n");
        printf("%s", CPU_RunInfo);
        printf("---------------------------------------------\r\n");
    }
}
#endif

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	assert( 0 );
}

void vApplicationTickHook(void)
{
    #if ENABLE_RTOS_MONITOR == 1
    CPU_RunTime++;
    #endif
}

static void cali_done_handle(CALI_HandleTypeDef *hcali, uint32_t result)
{
    system_set_LPRCCLK(cali_calc_rc_freq(hcali, result));
    system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_TYPE_CALIBRATION);
}

__RAM_CODE void hw_clock_init(void)
{
    System_ClkConfig_t sys_clk_cfg;

    sys_clk_cfg.SPLL_CFG.PLL_N = 8;
    sys_clk_cfg.SPLL_CFG.PLL_M = 0;
    sys_clk_cfg.SPLL_CFG.PowerEn = 1;
    sys_clk_cfg.MCU_Clock_Source = MCU_CLK_SEL_SPLL_CLK;
    sys_clk_cfg.SOC_DIV = 1;
    /* [eup-patch 2026-10-02 patch #7a] 96 MHz core; datasheet p.7 limit: 156 MHz. */
    sys_clk_cfg.MCU_DIV = 2;
    sys_clk_cfg.APB0_DIV = 1;
    sys_clk_cfg.APB1_DIV = 1;
    sys_clk_cfg.APB2_DIV = 1;
    System_SPLL_config(&sys_clk_cfg.SPLL_CFG, 1000);
    System_MCU_clock_Config(&sys_clk_cfg);

    __SYSTEM_UART_CLK_SELECT_SPLL();
}

__RAM_CODE void hw_xip_flash_init(bool wake_up)
{
    System_XIPConfig_t xip_config;
    xip_config.CLK_SRC_SEL = XIP_CLK_SEL_SPLL;
    xip_config.DIV_SEL = QSPI_BAUDRATE_DIV_4;
    xip_config.RD_TYPE = FLASH_RD_TYPE_DUAL;
    xip_config.WR_TYPE = FLASH_WR_TYPE_SINGLE;
    system_xip_flash_init(&xip_config,wake_up);
}

__RAM_CODE bool user_deep_sleep_check(void)
{
    return host_before_sleep_check();
}

__RAM_CODE void user_entry_before_sleep(void)
{
    while(!(Uart3_handle.UARTx->USR.TFE));
    system_delay_us(100);
    ool_write16(PMU_REG_PIN_PULL_EN, 0x3fff);
    ool_write16(PMU_REG_PIN_PULL_SEL, 0x3ffd);

    ool_write(PMU_REG_PMU_GATE_M, ool_read(PMU_REG_PMU_GATE_M) | 0x40);
}

__RAM_CODE void user_entry_after_sleep(void)
{
    /*
     * enable pull up of all 3.3v IO, these configuration will be latched by set
     * BIT6 of PMU_REG_PMU_GATE_M regsiter. used to avoid electric leakage
     */
    SYSTEM->PortA_PullSelect = 0x0000ffff;
    SYSTEM->PortB_PullSelect = 0x0000ffff;
    SYSTEM->PortC_PullSelect = 0x0000ffff;
    SYSTEM->PortD_PullSelect = 0x0000ffff;
    SYSTEM->PortE_PullSelect = 0x0000ffff;
    SYSTEM->PortA_PullEN = 0x0000ffff;
    SYSTEM->PortB_PullEN = 0x0000ffff;
    SYSTEM->PortC_PullEN = 0x0000ffff;
    SYSTEM->PortD_PullEN = 0x0000ffff;
    SYSTEM->PortE_PullEN = 0x0000ffff;
    SYSTEM->QspiPadConfig.QSPI_PullEN = 0x0000000;

    hw_clock_init();
    hw_xip_flash_init(true);
    host_hci_reinit();
    ool_write(PMU_REG_PMU_GATE_M, ool_read(PMU_REG_PMU_GATE_M) & (~0x40));

    GPIO_InitTypeDef gpio_config;

    NVIC_SetPriority(UART0_IRQn, 2);
    NVIC_EnableIRQ(UART0_IRQn);
    NVIC_SetPriority(PMU_IRQn, 4);
    NVIC_EnableIRQ(PMU_IRQn);

    /* configure PB4 and PB5 to UART3 function */
    __SYSTEM_GPIOA_CLK_ENABLE();
    gpio_config.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOB, &gpio_config);

    /* UART0: used for Log and AT command */
    __SYSTEM_UART3_CLK_ENABLE();
    Uart3_handle.UARTx = UART3;
    Uart3_handle.Init.BaudRate   = 9600;
    Uart3_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    Uart3_handle.Init.StopBits   = UART_STOPBITS_1;
    Uart3_handle.Init.Parity     = UART_PARITY_NONE;
    Uart3_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    Uart3_handle.TxCpltCallback  = NULL;
    Uart3_handle.RxCpltCallback  = app_at_rx_done;
    uart_init(&Uart3_handle);

    {
        static bool first_wakeup = true;
        bool do_calib = false;
        static TickType_t last_tick;
        TickType_t curr_tick;
        if (first_wakeup) {
            first_wakeup = false;
            last_tick = xTaskGetTickCount();
            curr_tick = last_tick;
            do_calib = true;
        }
        else {
            curr_tick = xTaskGetTickCount();
            if ((curr_tick - last_tick) > 10000) {
                last_tick = curr_tick;
                do_calib = true;
            }
        }

        if (do_calib) {
            /* restart calibration */
            __SYSTEM_CALI_CLK_ENABLE();
            cali_handle.mode = CALI_UP_MODE_NORMAL;
            cali_handle.rc_cnt = 60;
            cali_handle.DoneCallback = cali_done_handle;
            cali_init(&cali_handle);
            cali_start_IT(&cali_handle);
            system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_CALIBRATION);
            NVIC_SetPriority(CALI_IRQn, 2);
            NVIC_EnableIRQ(CALI_IRQn);
        }
    }
}

__RAM_CODE void user_entry_after_sleep_user(void)
{

}

static void app_hw_init(void)
{
    GPIO_InitTypeDef gpio_config;

    system_delay_us(1000000);

    /* configure all interrupt priority to 2 */
    *(volatile uint32_t *)0xE000E400 = 0x40404040;
    *(volatile uint32_t *)0xE000E404 = 0x40404040;
    *(volatile uint32_t *)0xE000E408 = 0x40404040;
    *(volatile uint32_t *)0xE000E40C = 0x40404040;
    *(volatile uint32_t *)0xE000E410 = 0x40404040;
    *(volatile uint32_t *)0xE000E414 = 0x40404040;
    *(volatile uint32_t *)0xE000E418 = 0x40404040;
    *(volatile uint32_t *)0xE000E41C = 0x40404040;
    *(volatile uint32_t *)0xE000E420 = 0x40404040;
    *(volatile uint32_t *)0xE000E424 = 0x40404040;
    *(volatile uint32_t *)0xE000E428 = 0x40404040;
    *(volatile uint32_t *)0xE000E42C = 0x40404040;
    *(volatile uint32_t *)0xE000E430 = 0x40404040;
    *(volatile uint32_t *)0xE000E434 = 0x40404040;
    *(volatile uint32_t *)0xE000E438 = 0x40404040;
    *(volatile uint32_t *)0xE000E43C = 0x40404040;
    *(volatile uint32_t *)0xE000E440 = 0x40404040;

    iwdt_Init_t boot_wdt={.iwdt_Count=32000u*60,.iwdt_Timeout=1024,.iwdt_int_Enable=WDT_INT_DISABLE};
    pmu_init();
    /* SDK pmu_init gates the inherited WDT clock: re-arm on every App boot. */
    iwdt_init(boot_wdt);
    iwdt_Enable();
    iwdt_Refresh();
//    /* Power Keep: 32KB PRAM, 96KB SRAM */
//    pmu_ram_retention_set(PMU_RAM_RET_CACHE_BIT
//                            | PMU_RAM_RET_CRAM0_BIT
//                            | PMU_RAM_RET_SRAM0_BIT
//                            | PMU_RAM_RET_SRAM1_BIT
//                            | PMU_RAM_RET_SRAM2_BIT);

    hw_clock_init();
    hw_xip_flash_init(false);

    /* configure PB4 and PB5 to UART3 function */
    gpio_config.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOB, &gpio_config);

    /* UART3: used for Log and AT command */
    __SYSTEM_UART3_CLK_ENABLE();
    Uart3_handle.UARTx = UART3;
    Uart3_handle.Init.BaudRate   = 9600;
    Uart3_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    Uart3_handle.Init.StopBits   = UART_STOPBITS_1;
    Uart3_handle.Init.Parity     = UART_PARITY_NONE;
    Uart3_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    Uart3_handle.TxCpltCallback  = NULL;
    Uart3_handle.RxCpltCallback  = app_at_rx_done;
    uart_init(&Uart3_handle);
    NVIC_SetPriority(UART3_IRQn, 4);
    NVIC_EnableIRQ(UART3_IRQn);

    /* do calibration, get current RC frequency */
    __SYSTEM_CALI_CLK_ENABLE();
    cali_handle.mode = CALI_UP_MODE_NORMAL;
    cali_handle.rc_cnt = 200;
    cali_handle.DoneCallback = cali_done_handle;
    cali_init(&cali_handle);
    cali_start_IT(&cali_handle);
    system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_CALIBRATION);
    NVIC_SetPriority(CALI_IRQn, 4);
    NVIC_EnableIRQ(CALI_IRQn);
}

int main( void )
{
#ifndef ENABLE_RTTHREAD
    app_hw_init();
#endif

    printf("start running\r\n");

    et100_sys_init();
    /* init flashdb to store user data */
    const int storage_ready=flashdb_init()==0;
    et100_serial_storage_ready(storage_ready);

    /* Create tasks */
#if ENABLE_RTOS_MONITOR == 1
    xTaskCreate(monitor_task, "monitor", MONITOR_TASK_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, &monitor_task_handle);
#endif

    /* create application task */
    app_task_init();

    /* initialize AT command */
    app_at_init(&Uart3_handle);
    et100_can_start(storage_ready);

    /*
     * enable pull up of all 3.3v IO, these configuration will be latched by set
     * BIT6 of PMU_REG_PMU_GATE_M regsiter. used to avoid electric leakage
     */
    SYSTEM->PortA_PullSelect = 0x0000ffff;
    SYSTEM->PortB_PullSelect = 0x00000fff;
    SYSTEM->PortC_PullSelect = 0x00000000;
    SYSTEM->PortD_PullSelect = 0x0000ffff;
    SYSTEM->PortE_PullSelect = 0x0000ffff;
    SYSTEM->PortA_PullEN = 0x0000ffff;
    SYSTEM->PortB_PullEN = 0x0000ffff;
    SYSTEM->PortC_PullEN = 0x0000ffff;
    SYSTEM->PortD_PullEN = 0x0000ffff;
    SYSTEM->PortE_PullEN = 0x0000ffff;
    SYSTEM->QspiPadConfig.QSPI_PullEN = 0x0000000;

    /* enable sleep */
    /* [eup-patch 2026-10-21 patch #8] keep sleep disabled on EVB dev build so SWD stays reachable
     * (DAPLink on FR3068EP-C DEV1.4 has no nRESET line for under-reset recovery). */
    /* system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_TYPE_DISABLE); */

    printf("FR5090: ble_simple_peripheral.\r\n");

    et100_status_led_init();

    /* Start the scheduler itself. */
	vTaskStartScheduler();

	return 0;
}

void uart3_irq(void)
{
    uart_IRQHandler(&Uart3_handle);
}

void PMU_GPIO_PMU_IRQHandler(void)
{
    uint16_t data = ool_read16(PMU_REG_PIN_DATA);
    uint16_t result = ool_read16(PMU_REG_PIN_XOR_RESULT);

    /* update last value with latest data */
    ool_write16(PMU_REG_PIN_LAST_V, data);
    /* clear last XOR result */
    ool_write16(PMU_REG_PIN_XOR_CLR, result);

    if (data & PMU_PIN_9) {
        system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_TYPE_HCI_RX);
    }
    else {
        system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_HCI_RX);
    }
}

void cali_irq(void)
{
    cali_IRQHandler(&cali_handle);
}

#ifdef ENABLE_RTTHREAD
void rt_hw_board_init(void)
{
    app_hw_init();
    rt_hw_interrupt_enable(0);

    ulTimerCountsForOneTick = ( system_get_CoreClock() / configTICK_RATE_HZ );
    xMaximumPossibleSuppressedTicks = 0xffffff / ulTimerCountsForOneTick;
    ulStoppedTimerCompensation = 45;

    SysTick_Config(system_get_CoreClock() / RT_TICK_PER_SECOND);
    /* set pend exception priority */
    NVIC_SetPriority(PendSV_IRQn, (1 << __NVIC_PRIO_BITS) - 1);

    rt_system_heap_init(&rt_heap[0], &rt_heap[configTOTAL_HEAP_SIZE/sizeof(uint32_t)]);
}

void rt_hw_console_output(const char *str)
{
    printf("%s", str);
}

__RAM_CODE void rt_system_power_manager(void)
{
    rt_tick_t xExpectedIdleTime;

    rt_base_t level = rt_hw_interrupt_disable();
    rt_tick_t next_timout = rt_timer_next_timeout_tick();
    if( next_timout >= rt_tick_get() )
        xExpectedIdleTime = next_timout - rt_tick_get();
    else
        xExpectedIdleTime = next_timout + (RT_TICK_MAX - rt_tick_get()) + 1;
    rt_hw_interrupt_enable(level);

    if (xExpectedIdleTime > 2) {
        vPortSuppressTicksAndSleep(xExpectedIdleTime);
    }
    else {
        printf("%d.\r\n", xExpectedIdleTime);
    }
}

/**
 * This is the timer interrupt service routine.
 *
 */
void SysTick_Handler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    rt_tick_increase();

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif // ENABLE_RTTHREAD
