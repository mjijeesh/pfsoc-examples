/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : cmd_hwinfo.c
 * Description: Header-driven hardware information command display generator.
 *******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include "hw_info.h"
#include "command.h"

/* Master configuration header generated dynamically per board from Libero XML */
#include "fpga_design_config/fpga_design_config.h"

/* Bit offsets defined in CONFIGURED_PERIPHERALS register XML */
#define PERIPH_EMMC_BIT      (1U << 0)
#define PERIPH_SD_SDIO_BIT   (1U << 1)
#define PERIPH_USB_BIT       (1U << 2)
#define PERIPH_MAC0_BIT      (1U << 3)
#define PERIPH_MAC1_BIT      (1U << 4)
#define PERIPH_QSPI_BIT      (1U << 5)
#define PERIPH_SPI0_BIT      (1U << 6)
#define PERIPH_SPI1_BIT      (1U << 7)
#define PERIPH_MMUART0_BIT   (1U << 8)
#define PERIPH_MMUART1_BIT   (1U << 9)
#define PERIPH_MMUART2_BIT   (1U << 10)
#define PERIPH_MMUART3_BIT   (1U << 11)
#define PERIPH_MMUART4_BIT   (1U << 12)
#define PERIPH_I2C0_BIT      (1U << 13)
#define PERIPH_I2C1_BIT      (1U << 14)
#define PERIPH_CAN0_BIT      (1U << 15)
#define PERIPH_CAN1_BIT      (1U << 16)
#define PERIPH_GPIO0_BIT     (1U << 17)
#define PERIPH_GPIO1_BIT     (1U << 18)
#define PERIPH_GPIO2_BIT     (1U << 19)

/**
 * @brief Displays target board design information.
 */
static void print_design_info(void)
{
    printf("  [Design Information]\n");
#ifdef LIBERO_SETTING_TARGET_DIE
    printf("    Target Part       : %s\n", LIBERO_SETTING_TARGET_DIE);
#elif defined(MPFS_TARGET_DIE) && defined(MPFS_TARGET_PACKAGE)
    printf("    Target Part       : %s (%s)\n", MPFS_TARGET_DIE, MPFS_TARGET_PACKAGE);
#else
    printf("    Target Part       : MPFS095T (FCSG325)\n");
#endif

#ifdef LIBERO_SETTING_DESIGN_NAME
    printf("    Design Name       : %s\n", LIBERO_SETTING_DESIGN_NAME);
#else
    printf("    Design Name       : MPFS_DISCOVERY_KIT_MSS\n");
#endif

#ifdef LIBERO_SETTING_LIBERO_VERSION
    printf("    Libero Version    : %s\n\n", LIBERO_SETTING_LIBERO_VERSION);
#else
    printf("    Libero Version    : N/A\n\n");
#endif
}

/**
 * @brief Displays system, bus, and CPU clock frequencies.
 */
static void print_clock_info(void)
{
    printf("  [System Clocks]\n");

#ifdef LIBERO_SETTING_MSS_COREPLEX_CPU_CLK
    printf("    CPU Coreplex Clock: %lu MHz (%lu Hz)\n",
           (unsigned long)(LIBERO_SETTING_MSS_COREPLEX_CPU_CLK / 1000000UL),
           (unsigned long)LIBERO_SETTING_MSS_COREPLEX_CPU_CLK);
#endif

#ifdef LIBERO_SETTING_MSS_SYSTEM_CLK
    printf("    System Clock      : %lu MHz (%lu Hz)\n",
           (unsigned long)(LIBERO_SETTING_MSS_SYSTEM_CLK / 1000000UL),
           (unsigned long)LIBERO_SETTING_MSS_SYSTEM_CLK);
#endif

#ifdef LIBERO_SETTING_MSS_AXI_CLK
    printf("    AXI Bus Clock     : %lu MHz (%lu Hz)\n",
           (unsigned long)(LIBERO_SETTING_MSS_AXI_CLK / 1000000UL),
           (unsigned long)LIBERO_SETTING_MSS_AXI_CLK);
#endif

#ifdef LIBERO_SETTING_MSS_APB_AHB_CLK
    printf("    APB/AHB Bus Clock : %lu MHz (%lu Hz)\n",
           (unsigned long)(LIBERO_SETTING_MSS_APB_AHB_CLK / 1000000UL),
           (unsigned long)LIBERO_SETTING_MSS_APB_AHB_CLK);
#endif

#ifdef LIBERO_SETTING_DDR_CLK
    printf("    DDR PHY Rate      : %lu MT/s (%lu Hz)\n\n",
           (unsigned long)(LIBERO_SETTING_DDR_CLK / 1000000UL),
           (unsigned long)LIBERO_SETTING_DDR_CLK);
#else
    printf("\n");
#endif
}

/**
 * @brief Displays macro-driven dynamic memory map calculations.
 */
static void print_memory_map_info(void)
{
    /* L2 Cache ways allocation (1 way = 128 KB, total capacity = 2048 KB) */
#ifdef LIBERO_SETTING_L2_WAY_ENABLE
    uint32_t cache_ways = (uint32_t)LIBERO_SETTING_L2_WAY_ENABLE;
#else
    uint32_t cache_ways = 1U; /* Standard 1 way (128 KB) enabled */
#endif

    uint32_t cache_kb = cache_ways * 128U;
    uint32_t lim_kb   = (2048U > cache_kb) ? (2048U - cache_kb) : 0U;
    uint32_t lim_end  = 0x08000000UL + (lim_kb * 1024UL) - 1UL;

    printf("  [Dynamic Memory Map]\n");
    printf("    L2-LIM RAM Size   : %u KB (0x08000000 - 0x%08X)\n", lim_kb, lim_end);
    printf("    L2 Cache Config   : %u KB (%u Way%s Enabled)\n", 
           cache_kb, cache_ways, (cache_ways > 1U) ? "s" : "");

#ifdef LIBERO_SETTING_DDR_CLK
#ifdef LIBERO_SETTING_DDR_SIZE
    uint32_t ddr_size_mb = (uint32_t)(LIBERO_SETTING_DDR_SIZE / (1024UL * 1024UL));
    uint32_t ddr_size_bytes = (uint32_t)LIBERO_SETTING_DDR_SIZE;
#else
    uint32_t ddr_size_mb = 128U; /* Default 128 MB for MPFS Discovery Kit */
    uint32_t ddr_size_bytes = ddr_size_mb * 1024U * 1024U;
#endif

    uint32_t cached_end = 0x80000000UL + ddr_size_bytes - 1UL;
    uint32_t noncached_end = 0xC0000000UL + ddr_size_bytes - 1UL;

    printf("    DDR Memory State  : ONLINE / TRAINED\n");
    printf("    DDR Cached Window : 0x80000000 - 0x%08X (%u MB)\n", cached_end, ddr_size_mb);
    printf("    DDR Non-Cached    : 0xC0000000 - 0x%08X (%u MB)\n", noncached_end, ddr_size_mb);
#else
    printf("    DDR Memory State  : OFFLINE / UNINITIALIZED\n");
#endif

    printf("\n");
}

/**
 * @brief Displays active MSS peripherals based on bitmask generated from Libero XML.
 */
static void print_configured_peripherals(void)
{
    printf("  [Configured MSS Peripherals]\n    Active: ");

#ifdef LIBERO_SETTING_CONFIGURED_PERIPHERALS
    uint32_t mask = LIBERO_SETTING_CONFIGURED_PERIPHERALS;

    if (mask & PERIPH_EMMC_BIT)    printf("eMMC ");
    if (mask & PERIPH_SD_SDIO_BIT) printf("SD/SDIO ");
    if (mask & PERIPH_USB_BIT)     printf("USB ");
    if (mask & PERIPH_MAC0_BIT)    printf("GEM0 ");
    if (mask & PERIPH_MAC1_BIT)    printf("GEM1 ");
    if (mask & PERIPH_QSPI_BIT)    printf("QSPI ");
    if (mask & PERIPH_SPI0_BIT)    printf("SPI0 ");
    if (mask & PERIPH_SPI1_BIT)    printf("SPI1 ");
    if (mask & PERIPH_MMUART0_BIT) printf("MMUART0 ");
    if (mask & PERIPH_MMUART1_BIT) printf("MMUART1 ");
    if (mask & PERIPH_MMUART2_BIT) printf("MMUART2 ");
    if (mask & PERIPH_MMUART3_BIT) printf("MMUART3 ");
    if (mask & PERIPH_MMUART4_BIT) printf("MMUART4 ");
    if (mask & PERIPH_I2C0_BIT)    printf("I2C0 ");
    if (mask & PERIPH_I2C1_BIT)    printf("I2C1 ");
    if (mask & PERIPH_CAN0_BIT)    printf("CAN0 ");
    if (mask & PERIPH_CAN1_BIT)    printf("CAN1 ");
    if (mask & PERIPH_GPIO0_BIT)   printf("GPIO0 ");
    if (mask & PERIPH_GPIO1_BIT)   printf("GPIO1 ");
    if (mask & PERIPH_GPIO2_BIT)   printf("GPIO2 ");
#else
    printf("None (LIBERO_SETTING_CONFIGURED_PERIPHERALS not defined)");
#endif

    printf("\n==================================================\n\n");
}

/**
 * @brief Prints hardware summary output.
 */
void show_hw_info(void)
{
    printf("==================================================\n");
    printf("        PolarFire SoC Hardware Description        \n");
    printf("==================================================\n");

    print_design_info();
    print_clock_info();
    print_memory_map_info();
    print_configured_peripherals();
}

/**
 * @brief Handler for 'hw_info' shell command.
 */
void hw_info_handler(int nb_params, char **params)
{
    (void)nb_params;
    (void)params;
    show_hw_info();
}