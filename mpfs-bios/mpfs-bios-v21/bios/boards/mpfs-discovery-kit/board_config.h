/*******************************************************************************
 * File Name  : boards/mpfs-discovery-kit/soc.h
 * Description: Board & SPI Flash Hardware Specifications for MPFS Discovery Kit
 *******************************************************************************/

#ifndef SOC_H_
#define SOC_H_

#include <stdint.h>

/* ========================================================================== */
/* 1. Dynamic Clock & System Board Details                                    */
/* ========================================================================== */
#if __has_include("hw_mss_clocks.h")
    #include "hw_mss_clocks.h"
#elif __has_include("clocks/hw_mss_clocks.h")
    #include "clocks/hw_mss_clocks.h"
#endif

#if defined(LIBERO_SETTING_MSS_CORE_CLOCK)
    #define CPU_FREQ_HZ                 LIBERO_SETTING_MSS_CORE_CLOCK
#elif defined(LIBERO_SETTING_CPU_CLOCK)
    #define CPU_FREQ_HZ                 LIBERO_SETTING_CPU_CLOCK
#else
    #define CPU_FREQ_HZ                 600000000UL   /* Default: 600 MHz */
#endif



/* Board Details */
#define BOARD_NAME                      "mpfs-discovery-kit"
#define MPFS_TARGET_DIE                 "MPFS095T"
#define MPFS_TARGET_PACKAGE             "FCSG325"

/* STDIO  UART Configuration */
#define MICROCHIP_STDIO_THRU_MMUARTX     &g_mss_uart0_lo
#define MICROCHIP_STDIO_BAUD_RATE        115200UL


/* ========================================================================== */
/* Console UART Hardware Configuration                                        */
/* ========================================================================== */
#ifndef CONSOLE_UART_INSTANCE
    #define CONSOLE_UART_INSTANCE       (&g_mss_uart0_lo)   /* MMUART0 */
#endif

#ifndef CONSOLE_UART_PERIPH
    #define CONSOLE_UART_PERIPH         MSS_PERIPH_MMUART0
#endif

#ifndef CONSOLE_UART_BAUD
    #define CONSOLE_UART_BAUD           MSS_UART_115200_BAUD
#endif




/* SPI Flash Controller Engine (Discovery Kit uses MSS SPI0) */
#ifndef FLASH_USE_MSS_SPI0
    #define FLASH_USE_MSS_SPI0
#endif

#ifndef FLASH_MSS_SPI_BASE_ADDR
    #define FLASH_MSS_SPI_BASE_ADDR     0x20100000UL
#endif

#ifndef FLASH_SPI_SLAVE_SELECT
    #define FLASH_SPI_SLAVE_SELECT      0
#endif

/* ========================================================================== */
/* SPI Flash Driver Family Definitions                                        */
/* ========================================================================== */
#define FLASH_FAMILY_AUTO               0              /* Dynamic JEDEC probe    */
#define FLASH_FAMILY_MICRON             1              /* Force Micron Driver    */
#define FLASH_FAMILY_SST                2              /* Force SST Driver       */
#define FLASH_FAMILY_WINBOND            3              /* Force Winbond Driver   */

/* Select Active Family: Change to FLASH_FAMILY_WINBOND, FLASH_FAMILY_SST, or FLASH_FAMILY_AUTO */
#ifndef SPI_FLASH_FAMILY
    //#define SPI_FLASH_FAMILY            FLASH_FAMILY_WINBOND
    #define SPI_FLASH_FAMILY            FLASH_FAMILY_AUTO    /* Auto Detect Winbond /SST/Miicron */
#endif

#if (SPI_FLASH_FAMILY == FLASH_FAMILY_WINBOND)
    #define SPI_FLASH_MODEL_NAME        "Winbond W25QXX Series Flash"
    #define SPI_FLASH_ADDRESS_BYTES     3
#elif (SPI_FLASH_FAMILY == FLASH_FAMILY_SST)
    #define SPI_FLASH_MODEL_NAME        "SST25 Series Flash"
    #define SPI_FLASH_ADDRESS_BYTES     3
#else
    #define SPI_FLASH_MODEL_NAME        "Auto-Detect SPI Flash"
    #define SPI_FLASH_ADDRESS_BYTES     3
#endif

#define SPI_FLASH_FRAME_BIT_SIZE        8u

#endif /* SOC_H_ */