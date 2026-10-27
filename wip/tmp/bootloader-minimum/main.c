#include "mss_uart.h"

#define LIM_ENTRY_ADDR 0x08002000

// Volatile variable to safely route the parked U54 cores
volatile unsigned long g_u54_jump_target = 0;

void release_u54_core(unsigned int hart_id, unsigned long destination_addr) {
    g_u54_jump_target = destination_addr;
    
    // Ensure the memory write is visible across all harts
    __asm__ volatile ("fence w,w" ::: "memory");

    // Signal the core via its specific CLINT MSIP register
    volatile unsigned int *clint_msip = (volatile unsigned int *)(0x02000000UL + (hart_id * 4));
    *clint_msip = 1;
}

void main(void) {
    uart_init();

    uart_puts("\r\n=== PolarFire SoC Ultra-Lean Bootloader ===\r\n");
    uart_puts("Awaiting: 'serialboot'\r\n");

    while (1) {
        if (check_command("serialboot")) {
            break;
        }
    }

    uart_puts("Cmd Ok. Send 4-Byte Payload Size Header...\r\n");

    unsigned int payload_size = 0;
    payload_size |= ((unsigned int)uart_getc() << 0);
    payload_size |= ((unsigned int)uart_getc() << 8);
    payload_size |= ((unsigned int)uart_getc() << 16);
    payload_size |= ((unsigned int)uart_getc() << 24);

    uart_puts("Streaming binary bytes directly to LIM...\r\n");

    unsigned char *destination = (unsigned char *)LIM_ENTRY_ADDR;
    for (unsigned int i = 0; i < payload_size; i++) {
        *destination++ = uart_getc();
    }

    uart_puts("Releasing U54 Core 1 to run application code from LIM...\r\n\r\n");
    release_u54_core(1, LIM_ENTRY_ADDR);

    // Park the E51 core into a low-power wait state
    while (1) {
        __asm__ volatile ("wfi");
    }
}