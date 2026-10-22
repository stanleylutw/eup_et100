#include "fr30xx.h"
#include "mcuboot_image.h"

/* No C stack access is permitted after replacing MSP. Never returns to stub PRAM. */
__attribute__((naked,noreturn)) static void enter_app(uint32_t sp __attribute__((unused)),
                                                        uint32_t reset __attribute__((unused)))
{
    __asm volatile(
        "movs r2, #0\n"
        "msr control, r2\n"
        "isb\n"
        "msr basepri, r2\n"
        "msr faultmask, r2\n"
        "msr msplim, r2\n"
        "msr psplim, r2\n"
        "msr psp, r2\n"
        "msr msp, r0\n"
        "dsb\n"
        "isb\n"
        "bx r1\n");
}
__attribute__((noreturn)) void installer_jump(const struct image_info *info)
{
    uint32_t sp=info->msp, reset=info->reset;
    __disable_irq();
    SysTick->CTRL=0;SysTick->LOAD=0;SysTick->VAL=0;
    unsigned banks=(SCnSCB->ICTR&15u)+1;
    if (banks>8) banks=8;
    for (unsigned i=0;i<banks;i++) { NVIC->ICER[i]=UINT32_MAX;NVIC->ICPR[i]=UINT32_MAX; }
    SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk|SCB_ICSR_PENDSVCLR_Msk;
    /* SDK baud reconfiguration can retain the loader divisor without a reset. */
    __SYSTEM_UART3_RESET();
    SCB->VTOR=ET100_APP_VECTOR;
    __DSB();__ISB();
    enter_app(sp,reset);
}
