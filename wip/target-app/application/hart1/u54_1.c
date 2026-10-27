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
    printf("  Microchip PolarFire SoC BareMetal BIOS\n");
    printf("  --------------------------------------------------\n");
    printf("  Version                  : V1.0.0\n");
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

void u54_1(void)
{
    main(0, NULL);
}

uint32_t count_sw_ints_h1 = 0U;

void Software_h1_IRQHandler(void)
{
    count_sw_ints_h1++;
}