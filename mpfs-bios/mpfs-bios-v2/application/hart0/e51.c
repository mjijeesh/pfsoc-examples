#include "mpfs_hal/mss_hal.h"
#include "uart.h"
#include "cli.h"

#include "drivers/mss/mss_mmuart/mss_uart.h"

volatile uint32_t count_sw_ints_h0 = 0U;



const uint8_t g_message3[] =  " \r\n\r\n---------BIOS Testing  --------\r\n\r\n  ";

void e51(void)
{
    // Initialize MMUART0 & Clock via HAL wrapper
    uart_init();





    /* Message on uart0 */
    MSS_UART_polled_tx(&g_mss_uart0_lo, g_message3,
            sizeof(g_message3));



    // Clear software interrupts (keep application cores parked)
    clear_soft_interrupt();

    // Launch LiteX CLI prompt
    cli_init();
    cli_run();

    /* Never reached */
}

void Software_h0_IRQHandler(void)
{
    count_sw_ints_h0++;
}