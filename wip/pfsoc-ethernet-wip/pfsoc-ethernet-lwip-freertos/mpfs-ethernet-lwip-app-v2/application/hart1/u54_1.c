#include "mpfs_hal/mss_hal.h"
#include "uart.h"
#include "cli.h"

#include "drivers/mss/mss_mmuart/mss_uart.h"

volatile uint32_t count_sw_ints_h1 = 0U;

void u54_1(void)
{
    // Initialize MMUART0 & Clock via HAL wrapper
    uart_init();



    // Clear software interrupts (keep application cores parked)
    clear_soft_interrupt();

    // Launch LiteX CLI prompt
    cli_init();
    cli_run();

    /* Never reached */
}

void Software_h1_IRQHandler(void)
{
    count_sw_ints_h1++;
}