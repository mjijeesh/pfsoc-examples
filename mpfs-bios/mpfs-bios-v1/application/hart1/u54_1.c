#include "mpfs_hal/mss_hal.h"

void u54_1(void) 
{
    clear_soft_interrupt();
    set_csr(mie, MIP_MSIP);

    while (1) {
        __asm__ volatile ("wfi");
    }
}

void U54_1_software_IRQHandler(void)
{
    clear_soft_interrupt();
}