#include <stdio.h>
#include <stdint.h>
#include "command.h"
#include "mpfs_hal/mss_hal.h"
#include "uart.h"

extern void uart_sync(void);
extern void __disable_irq(void);

void help_handler(int nb_params, char **params)
{
    (void)nb_params; (void)params;
    command_help_show();
}

void ident_handler(int nb_params, char **params)
{
    (void)nb_params; (void)params;
    printf("Ident: LiteX BIOS on Microchip PolarFire SoC (E51 Core)\n");
}

void reboot_handler(int nb_params, char **params)
{
    (void)nb_params;
    (void)params;

    printf("\nRebooting system...\n");
    uart_sync();

    /* Disable interrupts */
    __disable_irq();

    /* 
     * Trigger PolarFire SoC System Soft Reset.
     * Writing magic key 0xDEAD to SYSREG->MSS_RESET_CR (0x20002018) triggers 
     * a full hardware reset across CPU coreplex, L2 cache, and peripherals.
     */
    *(volatile uint32_t *)(0x20002018UL) = 0xDEADUL;

    while (1) {
        /* Wait for System Controller to reset hardware */
    }
}