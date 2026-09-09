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

void u54_1(void) {
    write_csr(mscratch, 0);
    write_csr(mcause, 0);
    write_csr(mepc, 0);
    PLIC_init();

    SYSREG->SOFT_RESET_CR = 0U;
    SYSREG->SUBBLK_CLOCK_CR = 0xFFFFFFFFUL;

    /* Remove: __disable_local_irq((int8_t)MMUART0_E51_INT); */

    MSS_UART_init(DEMO_UART, MSS_UART_115200_BAUD, MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
    PRINT_STRING("\r\n=======================================================\r\n");
    PRINT_STRING(" PolarFire SoC - FreeRTOS + lwIP Network Engine (U54_1)\r\n");
    PRINT_STRING("=======================================================\r\n");

    extern void freertos_risc_v_trap_handler(void);
    write_csr(mtvec, (uint64_t)freertos_risc_v_trap_handler);

    low_level_init();

    g_rx_sem = xSemaphoreCreateBinary();
    tcpip_init(tcpip_init_done_cb, NULL);

    xTaskCreate(eth_rx_task, "ETH_RX", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(phy_monitor_task, "PHY_MON", 2024, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(cli_task, "CLI_TASK", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);

    PLIC_SetPriority(MAC0_INT_U54_INT, 2);
    PLIC_EnableIRQ(MAC0_INT_U54_INT);

    PRINT_STRING("[INIT] Starting FreeRTOS Kernel Scheduler on U54_1...\r\n");
    vTaskStartScheduler();

    while (1);
}

/* Do NOT include int main(void) here */

/* FreeRTOS Required Application Hook Functions */
void vAssertCalled(void) { taskDISABLE_INTERRUPTS(); for (;;); }
void vApplicationMallocFailedHook(void) { taskDISABLE_INTERRUPTS(); for (;;); }
//void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) { (void)xTask; (void)pcTaskName; taskDISABLE_INTERRUPTS(); for (;;); }

extern void handle_m_ext_interrupt(void);
void freertos_risc_v_application_interrupt_handler(uint32_t mcause) {
    (void)mcause;
    handle_m_ext_interrupt();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    taskDISABLE_INTERRUPTS();
    PRINT_STRING("\r\n[CRITICAL ERROR] FreeRTOS Stack Overflow in Task: ");
    if (pcTaskName != NULL) {
        PRINT_STRING(pcTaskName);
    }
    PRINT_STRING("\r\nSystem Halting.\r\n");
    for (;;);
}
