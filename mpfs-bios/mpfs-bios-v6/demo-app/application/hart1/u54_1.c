#include "mpfs_hal/mss_hal.h"

void u54_1(void) 
{
    clear_soft_interrupt();
    set_csr(mie, MIP_MSIP);

    while (1) {
        __asm__ volatile ("wfi");
    }
}

// Correct HAL weak symbol name
void Software_h1_IRQHandler(void)
{
    clear_soft_interrupt();
}



/* This is the handler function for the software interrupt on hart1.
 * In this example project hart1 is woken up from WFI by hart0 using IPI.
 */

 /*
void u54_1(void) 
{
    // 1. Clear the startup IPI sent by Hart 0 during app initialization
    clear_soft_interrupt();

    // 2. Perform Hart 1 application tasks
    while (1) {
        // Your processing/worker code for U54 Core 1 goes here
    }
}
*/