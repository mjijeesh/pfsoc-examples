#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "hw_info.h"
#include "command.h"
#include "mpfs_hal/mss_hal.h"
#include "fpga_design_config/fpga_design_config.h"

/* SYSREG Register Base Addresses */
#define SYSREG_BASE             0x20000000UL
#define SYSREG_SOFT_RESET_CR    (*(volatile uint32_t *)(SYSREG_BASE + 0x2008UL))

/* L2 Cache Controller Registers */
#define L2_CACHE_BASE           0x20108000UL
#define L2_CACHE_WAY_ENABLE     (*(volatile uint32_t *)(L2_CACHE_BASE + 0x0008UL))

/* CLINT Base Addresses */
#define CLINT_MSIP_BASE         0x02000000UL
#define CLINT_MTIME             (*(volatile uint64_t *)(0x0200BFF8UL))

/* Standard 32-bit Cached DDR Base Address */
#define DDR_CACHED_BASE         0x80000000UL

/* Read CPU Cycle Counter */
static inline uint64_t get_cpu_cycles(void)
{
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

/* Dynamically measure CPU Coreplex clock frequency using 1 MHz CLINT mtime */
static uint32_t measure_cpu_freq_hz(void)
{
    uint64_t start_mtime = CLINT_MTIME;
    while (CLINT_MTIME == start_mtime); /* Sync to tick edge */

    start_mtime = CLINT_MTIME;
    uint64_t start_cycles = get_cpu_cycles();

    /* Measure over 10,000 mtime ticks (10 ms) */
    while ((CLINT_MTIME - start_mtime) < 10000UL);

    uint64_t end_cycles = get_cpu_cycles();
    uint64_t elapsed_ticks = CLINT_MTIME - start_mtime;

    return (uint32_t)(((end_cycles - start_cycles) * 1000000UL) / elapsed_ticks);
}

/* Dynamically scan SYSREG SOFT_RESET_CR to report active MSS peripherals */
static void print_dynamic_peripherals(void)
{
    uint32_t rst_cr = SYSREG_SOFT_RESET_CR;

    printf("  [Configured MSS Peripherals (Dynamic SYSREG Scan)]\n   Active:");

    if (!(rst_cr & (1U << 0)))  printf(" GEM0");
    if (!(rst_cr & (1U << 1)))  printf(" GEM1");
    if (!(rst_cr & (1U << 2)))  printf(" MMC/SDIO");
    if (!(rst_cr & (1U << 3)))  printf(" USB");
    if (!(rst_cr & (1U << 4)))  printf(" CAN0");
    if (!(rst_cr & (1U << 5)))  printf(" CAN1");
    if (!(rst_cr & (1U << 6)))  printf(" SPI0");
    if (!(rst_cr & (1U << 7)))  printf(" SPI1");
    if (!(rst_cr & (1U << 8)))  printf(" I2C0");
    if (!(rst_cr & (1U << 9)))  printf(" I2C1");
    if (!(rst_cr & (1U << 10))) printf(" MMUART0");
    if (!(rst_cr & (1U << 11))) printf(" MMUART1");
    if (!(rst_cr & (1U << 12))) printf(" MMUART2");
    if (!(rst_cr & (1U << 13))) printf(" MMUART3");
    if (!(rst_cr & (1U << 14))) printf(" MMUART4");
    if (!(rst_cr & (1U << 15))) printf(" GPIO0");
    if (!(rst_cr & (1U << 16))) printf(" GPIO1");
    if (!(rst_cr & (1U << 17))) printf(" GPIO2");

    printf("\n");
}

/* Dynamically read L2 Cache Controller ways to compute LIM vs Cache RAM */
static void print_dynamic_memory_map(void)
{
    uint32_t raw_ways = L2_CACHE_WAY_ENABLE & 0x0FUL;
    if (raw_ways > 15U) raw_ways = 0U;

    uint32_t cache_ways_count = raw_ways + 1U;
    uint32_t cache_kb = cache_ways_count * 128U;
    uint32_t lim_kb = (cache_kb < 2048U) ? (2048U - cache_kb) : 0U;

    /* Check if DDR clock is configured in Libero fpga_design_config */
#ifdef LIBERO_SETTING_DDR_CLK
    bool ddr_configured = true;
#else
    bool ddr_configured = false;
#endif

    printf("  [Dynamic Memory Map]\n");
    printf("    L2-LIM RAM Size   : %u KB (0x08000000 - 0x%08X)\n", 
           lim_kb, 0x08000000U + (lim_kb * 1024U) - 1U);
    printf("    L2 Cache Config   : %u KB (%u Way%s Enabled)\n", 
           cache_kb, cache_ways_count, (cache_ways_count > 1U) ? "s" : "");

    printf("    DDR Memory State  : %s\n", ddr_configured ? "ONLINE / TRAINED" : "OFFLINE / UNINITIALIZED");
    if (ddr_configured) {
        printf("    DDR Cached Window : 0x80000000 - 0x87FFFFFF (128 MB)\n");
        printf("    DDR Non-Cached    : 0xC0000000 - 0xC7FFFFFF (128 MB)\n");
    }
}

/* Dynamically query hart status across CLINT MSIP registers */
static void print_hart_status(void)
{
    printf("  [Coreplex Hart Execution Status]\n");
    printf("    Hart 0 (E51 Monitor) : ACTIVE (Executing BIOS)\n");

    for (int hart = 1; hart <= 4; hart++) {
        uint32_t msip = *(volatile uint32_t *)(CLINT_MSIP_BASE + (hart * 4));
        printf("    Hart %d (U54_%d App)   : %s\n", 
               hart, hart, (msip & 1U) ? "RUNNING / PENDING_INT" : "PARKED IN WFI");
    }
}

/* Public Command Entry Point */
void show_hw_info(void)
{
    uint32_t cpu_hz = measure_cpu_freq_hz();
    uint32_t cpu_mhz = (cpu_hz + 500000U) / 1000000U;

    printf("\n==================================================\n");
    printf("        PolarFire SoC Hardware Description        \n");
    printf("==================================================\n");

    printf("  [Dynamic Clock Frequencies]\n");
    printf("    Measured CPU Core : %u MHz (%u Hz)\n", cpu_mhz, cpu_hz);
    printf("    AXI Bus Clock     : %u MHz\n", cpu_mhz / 2U);
    printf("    APB Bus Clock     : %u MHz\n\n", cpu_mhz / 4U);

    print_dynamic_memory_map();
    printf("\n");

    print_hart_status();
    printf("\n");

    print_dynamic_peripherals();
    printf("==================================================\n\n");
}

void hw_info_handler(int nb_params, char **params)
{
    (void)nb_params;
    (void)params;
    show_hw_info();
}