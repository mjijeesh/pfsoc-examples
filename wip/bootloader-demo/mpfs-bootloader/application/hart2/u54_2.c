#include "mpfs_hal/mss_hal.h"

extern volatile uint64_t g_app_jump_addr;

typedef void (*entry_func_t)(unsigned long r1, unsigned long r2, unsigned long r3);

void u54_2(void) 
{
    clear_soft_interrupt();
    set_csr(mie, MIP_MSIP);

    while (1) {
        // 1. Wait for IPI from Hart 0
        __asm__ volatile ("wfi");

        // 2. Wake up and check if Hart 0 published a target boot address
        if (g_app_jump_addr != 0) {
            clear_soft_interrupt();
            entry_func_t entry = (entry_func_t)(uintptr_t)g_app_jump_addr;

            asm volatile("fence" ::: "memory");
            asm volatile("fence.i" ::: "memory");

            // 3. Jump directly to target application startup (_start at 0x08040000)
            entry(0, 0, 0);
        }
    }
}

void Software_h2_IRQHandler(void)
{
    clear_soft_interrupt();
}