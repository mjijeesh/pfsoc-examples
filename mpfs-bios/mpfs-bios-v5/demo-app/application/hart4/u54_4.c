#include "mpfs_hal/mss_hal.h"

void u54_4(void) 
{
    clear_soft_interrupt();
    set_csr(mie, MIP_MSIP);

    while (1) {
        __asm__ volatile ("wfi");
    }
}

void U54_4_software_IRQHandler(void)
{
    clear_soft_interrupt();
}