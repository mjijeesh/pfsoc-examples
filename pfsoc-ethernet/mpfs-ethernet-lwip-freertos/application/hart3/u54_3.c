#include "mpfs_hal/mss_hal.h"

void u54_3(void) 
{
    clear_soft_interrupt();
    set_csr(mie, MIP_MSIP);

    while (1) {
        __asm__ volatile ("wfi");
    }
}

// Correct HAL weak symbol name
void Software_h3_IRQHandler(void)
{
    clear_soft_interrupt();
}