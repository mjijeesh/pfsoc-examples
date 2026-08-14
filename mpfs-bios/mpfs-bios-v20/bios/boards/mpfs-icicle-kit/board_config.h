/*******************************************************************************
 * File Name  : boards/mpfs-icicle-kit/soc.h
 * Description: Board & SPI Flash Hardware Specifications for MPFS Icicle Kit
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

#define BOARD_NAME                      "mpfs-icicle-kit"
#define MPFS_TARGET_DIE                 "MPFS250T"
#define MPFS_TARGET_PACKAGE             "FCVG484"

/* Console UART Configuration */
#define MICROCHIP_STDIO_THRU_MMUARTX     &g_mss_uart0_lo
#define MSS_PERIPH_MMUART_E51            MSS_PERIPH_MMUART0
#define CONSOLE_BAUD_RATE                115200UL

/* ========================================================================== */
/* SPI Flash Driver Family Definitions  for ICICLE-KIT                        */
/* ========================================================================== */

#define USE_CORE_SPI                               /* FPGA CoreSPI IP Engine */
#define CORESPI_BASE_ADDR           0x40000000UL   /* CoreSPI Base Address   */
#define CORESPI_SLAVE_SELECT        1              /* Slave Select Index 1   */



/* SPI Flash Controller Engine (ICICLE Kit uses  CORESPI on SPI0 connected to PF_SPI port) */
#ifndef FLASH_USE_CORE_SPI
    #define FLASH_USE_CORE_SPI                               /* Use CoreSPI Engine */
#endif

#ifndef FLASH_CORESPI_BASE_ADDR
    #define FLASH_CORESPI_BASE_ADDR           0x40000000UL   /* CoreSPI Base Address   */
#endif

#ifndef FLASH_SPI_SLAVE_SELECT
    #define FLASH_SPI_SLAVE_SELECT      1                    /* Slave Select Index 1   */
#endif

/* Flash Driver Declarations */
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