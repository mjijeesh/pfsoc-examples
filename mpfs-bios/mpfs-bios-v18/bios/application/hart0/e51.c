/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * File Name  : e51.c
 * Description: Primary entry point and orchestration loop for E51 Hart 0.
 *******************************************************************************/

#include <stdio.h>
#include "mpfs_hal/mss_hal.h"
#include "uart.h"
#include "readline.h"
#include "command.h"
#include "boot.h"
#include "hw_info.h"
#include "cli.h"

/**
 * @brief Prints the startup logo and system metadata.
 */
static void print_bios_banner(void)
{
    printf("\n");
    printf("  ____  ___  _     _     ____  _____ ___ ____  _____   ____   ___   ____ \n");
    printf(" |  _ \\/ _ \\| |   / \\   |  _ \\|  ___|_ _|  _ \\| ____| / ___| / _ \\ / ___|\n");
    printf(" | |_) | | | | |  / _ \\  | |_) | |_   | || |_) |  _|   \\___ \\| | | | |    \n");
    printf(" |  __/| |_| | |_/ ___ \\ |  _ <|  _|  | ||  _ <| |___   ___) | |_| | |___\n");
    printf(" |_|    \\___/|____/_/   \\_\\|_| \\_\\|_|   |___|_| \\_____| |____/ \\___/ \\____|\n");
    printf("\n");
    printf("  Microchip PolarFire SoC BareMetal BIOS\n");
    printf("  --------------------------------------------------\n");
    printf("  Hart0E51MonitorCore      : Active\n");
    printf("  Harts1To4U54Application  : Parked In WFI\n");
    printf("  Memory Map               : LIM(0x08000000) | DDR(0x80000000)\n");
    printf("  Build Date & Time        : " __DATE__ " " __TIME__ "\n\n");
    fflush(stdout);
}

/**
 * @brief Primary BareMetal BIOS entry point on Hart 0.
 */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    /* 1. Hardware Drivers & History Buffer Initialization */
    uart_init();
    hist_init();

    /* 2. System Identification & Hardware Info */
    print_bios_banner();
    clear_soft_interrupt();
    show_hw_info();

    /* 3. Autoboot Sequence (Countdown -> Serialboot -> Flashboot) */
    autoboot_run(10);

    /* 4. Interactive CLI Shell Loop */
    cli_loop();

    return 0;
}

void e51(void)
{
    main(0, NULL);
}

/**
 * @brief Hart 0 local software interrupt service handler.
 */
void Software_h0_IRQHandler(void)
{
    clear_soft_interrupt();
}