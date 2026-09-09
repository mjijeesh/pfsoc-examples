/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * File Name  : e51.c
 * Description: Primary entry point and orchestration loop for E51 Hart 0.
 *******************************************************************************/

#include <stdio.h>
#include "mpfs_hal/mss_hal.h"
#include "board_config.h"        /* Pulled automatically from boards/$(BOARD)/soc.h */
#include "uart.h"
#include "readline.h"
#include "command.h"
#include "boot.h"
#include "hw_info.h"
#include "cli.h"
#include "spi_flash.h"

#ifndef BOARD_NAME
#define BOARD_NAME "mpfs-discovery-kit"
#endif

#ifndef MEM_TARGET
#define MEM_TARGET "unknown"
#endif

#ifndef CPU_FREQ_HZ
#define CPU_FREQ_HZ 600000000UL
#endif

/**
 * @brief Prints system metadata, target board information, and peripheral states.
 */
void print_system_info(void)
{
    uint32_t mhartid;
    char flash_size_str[32];
    
    __asm__ volatile ("csrr %0, mhartid" : "=r"(mhartid));

    /* Get formatted flash size string (e.g. "16 MB (16384 KB)") */
    spi_flash_get_formatted_size(flash_size_str, sizeof(flash_size_str));

    printf("\r\n==================================================");
    printf("\r\n      PolarFire SoC Bare-Metal BIOS v1.0");
    printf("\r\n==================================================");
    printf("\r\n Board Name       : %s", BOARD_NAME);
#if defined(MPFS_TARGET_DIE) && defined(MPFS_TARGET_PACKAGE)
    printf("\r\n FPGA Target      : %s-%s", MPFS_TARGET_DIE, MPFS_TARGET_PACKAGE);
#endif
    printf("\r\n Active Hart ID   : Hart %u (%s)", 
           (unsigned int)mhartid, (mhartid == 0) ? "E51 Monitor" : "U54 Application Core");
    printf("\r\n CPU Frequency    : %u MHz", (unsigned int)(CPU_FREQ_HZ / 1000000UL));

    printf("\r\n\n--- Peripheral & Flash Hardware ---");
#if defined(FLASH_USE_CORE_SPI)
    printf("\r\n SPI Interface    : FPGA CoreSPI IP (Base: 0x%08X)", 
           (unsigned int)FLASH_CORESPI_BASE_ADDR);
#elif defined(FLASH_USE_MSS_SPI1)
    printf("\r\n SPI Interface    : Hard MSS SPI1 (Base: 0x20101000)");
#elif defined(FLASH_USE_MSS_SPI0)
    printf("\r\n SPI Interface    : Hard MSS SPI0 on MikroBus (Base: 0x20100000)");
#else
    printf("\r\n SPI Interface    : Hard MSS SPI0 (Base: 0x20100000)");
#endif

    printf("\r\n Detected Flash   : %s [%s]", spi_flash_get_driver_name(), flash_size_str);

    printf("\r\n\n--- Active Memory Region ---");
    printf("\r\n Build Target     : %s", MEM_TARGET);

#if defined(BUILD_TIMESTAMP)
    printf("\r\n\n Build Timestamp  : %s", BUILD_TIMESTAMP);
#else
    printf("\r\n\n Build Date       : %s %s", __DATE__, __TIME__);
#endif
    printf("\r\n==================================================\r\n");
}

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

    /* 1. Initialize MMUART console */
    uart_init();
    hist_init();

    /* 2. Auto-detect & initialize target SPI Flash memory */
    spi_flash_init();

    /* 3. Print complete board & peripheral diagnostic banner */
    print_system_info();

    /* 4. Launch Autoboot countdown */
    autoboot_run(10);

    /* 5. Enter interactive CLI loop */
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