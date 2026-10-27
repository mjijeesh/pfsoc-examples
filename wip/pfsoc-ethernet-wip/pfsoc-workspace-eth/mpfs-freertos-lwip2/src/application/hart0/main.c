/*******************************************************************************
 * PolarFire SoC Discovery Kit - Clean System Orchestrator
 *******************************************************************************/

#include "mpfs_hal/mss_hal.h"
#include "mpfs_hal/common/nwc/mss_nwc_init.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lwip/tcpip.h"

#include "app_shared.h"
#include "net_tasks.h"
#include "uart_cli.h"

void e51(void) {
    write_csr(mscratch, 0);
    write_csr(mcause, 0);
    write_csr(mepc, 0);
    PLIC_init();

    SYSREG->SOFT_RESET_CR = 0U;
    SYSREG->SUBBLK_CLOCK_CR = 0xFFFFFFFFUL;

    __disable_local_irq((int8_t)MMUART0_E51_INT);

    MSS_UART_init(DEMO_UART, MSS_UART_115200_BAUD, MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
    PRINT_STRING("\r\n=======================================================\r\n");
    PRINT_STRING(" PolarFire SoC - FreeRTOS + lwIP Network Engine\r\n");
    PRINT_STRING("=======================================================\r\n");

    PRINT_STRING("[INIT] Configured MMUART0 at 115200 Baud.\r\n");

    extern void freertos_risc_v_trap_handler(void);
    write_csr(mtvec, (uint64_t)freertos_risc_v_trap_handler);
    PRINT_STRING("[INIT] Machine Trap Vector (mtvec) assigned to FreeRTOS Handler.\r\n");

    PRINT_STRING("[INIT] Initializing Low-Level GEM0 MAC & VSC8221 PHY...\r\n");
    low_level_init();

    PRINT_STRING("[INIT] Creating Ethernet RX Semaphore...\r\n");
    g_rx_sem = xSemaphoreCreateBinary();

    PRINT_STRING("[INIT] Launching lwIP Core (tcpip_init)...\r\n");
    tcpip_init(tcpip_init_done_cb, NULL);

    PRINT_STRING("[INIT] Creating FreeRTOS Tasks...\r\n");
    xTaskCreate(eth_rx_task, "ETH_RX", 1024, NULL, configMAX_PRIORITIES - 1, NULL);
    PRINT_STRING("  -> Created Task: ETH_RX (Priority: High)\r\n");

    xTaskCreate(phy_monitor_task, "PHY_MON", 512, NULL, tskIDLE_PRIORITY + 2, NULL);
    PRINT_STRING("  -> Created Task: PHY_MON (Priority: Medium)\r\n");

    xTaskCreate(cli_task, "CLI_TASK", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
    PRINT_STRING("  -> Created Task: CLI_TASK (Priority: Low)\r\n");

    PRINT_STRING("[INIT] Enabling GEM0 Ethernet Interrupts in PLIC...\r\n");
    PLIC_SetPriority(MAC0_INT_U54_INT, 2);
    PLIC_EnableIRQ(MAC0_INT_U54_INT);

    __enable_irq();
    PRINT_STRING("[INIT] Global interrupts enabled.\r\n");

    PRINT_STRING("[INIT] Starting FreeRTOS Kernel Scheduler...\r\n");
    vTaskStartScheduler();

    while (1);
}

int main(void) {
    e51();
    return 0;
}

/* FreeRTOS Required Application Hook Functions */
void vAssertCalled(void) { taskDISABLE_INTERRUPTS(); for (;;); }
void vApplicationMallocFailedHook(void) { taskDISABLE_INTERRUPTS(); for (;;); }
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) { (void)xTask; (void)pcTaskName; taskDISABLE_INTERRUPTS(); for (;;); }

extern void handle_m_ext_interrupt(void);
void freertos_risc_v_application_interrupt_handler(uint32_t mcause) {
    (void)mcause;
    handle_m_ext_interrupt();
}
