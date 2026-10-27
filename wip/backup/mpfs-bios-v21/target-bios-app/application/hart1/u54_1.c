#include <stdio.h>
#include "mpfs_hal/mss_hal.h"
#include "board_config.h"
#include "uart.h"
#include "cli.h"

#include "drivers/mss/mss_mmuart/mss_uart.h"

/* Global target jump address published by boot() in boot.c */
extern volatile uint64_t g_app_jump_addr;
volatile uint32_t count_sw_ints_h1 = 0U;

/**
 * @brief Default weak entry point for Target Application mode.
 */
__attribute__((weak)) void application_main(void)
{
    printf("[ APP ] DDR Application Payload running on U54_1 Core...\n");
}

/**
 * @brief Hart 1 Primary Entry Point
 */
void u54_1(void)
{
    /* Clear any initial pending software interrupt */
    clear_soft_interrupt();

    /* Read dynamic Hart ID from mhartid CSR via HAL macro */
    uint32_t hartid = (uint32_t)read_csr(mhartid);

    /* Resolve active MMUART peripheral number */
    uint32_t uart_num = 0U;
    if (CONSOLE_UART_PERIPH == MSS_PERIPH_MMUART1) {
        uart_num = 1U;
    } else if (CONSOLE_UART_PERIPH == MSS_PERIPH_MMUART2) {
        uart_num = 2U;
    } else if (CONSOLE_UART_PERIPH == MSS_PERIPH_MMUART3) {
        uart_num = 3U;
    } else if (CONSOLE_UART_PERIPH == MSS_PERIPH_MMUART4) {
        uart_num = 4U;
    }

    /* ================================================================== */
    /* DDR TARGET APPLICATION EXECUTION PATH (Hart 1 / U54_1)             */
    /* ================================================================== */

    /* 1. Initialize Console UART for Hart 1 */
    uart_init();

    /* 2. Print Target Application Header */
    printf("\n==================================================\n");
    printf(" PolarFire SoC DDR Target Application | %s\n", BOARD_NAME);
    printf("==================================================\n");
    printf(" [ SYSTEM ] Core / Hart    : Hart %u (U54_%u Application Core)\n", hartid, hartid);
    printf(" [ SYSTEM ] Execution RAM  : 0x%08X (DDR Memory)\n", (unsigned int)PAYLOAD_RAM_ADDR);
    printf(" [   OK   ] MMUART Console : MMUART%u (%lu bps)\n", uart_num, (unsigned long)CONSOLE_UART_BAUD);
#if defined(BUILD_TIMESTAMP)
    printf(" [ BUILD  ] Build Timestamp: %s\n", BUILD_TIMESTAMP);
#else
    printf(" [ BUILD  ] Build Date     : %s %s\n", __DATE__, __TIME__);
#endif
    printf("==================================================\n\n");
    uart_sync();

    /* 3. Run Application Payload Logic */
    application_main();

    /* 4. Launch Interactive CLI Shell on Hart 1 */
    cli_loop();
}

/**
 * @brief Hart 1 local software interrupt service handler.
 */
void Software_h1_IRQHandler(void)
{
    count_sw_ints_h1++;
    /* Clear software interrupt to prevent infinite IRQ loop */
    clear_soft_interrupt();
}