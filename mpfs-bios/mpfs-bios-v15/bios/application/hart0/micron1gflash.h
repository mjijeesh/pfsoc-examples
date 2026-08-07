/***************************************************************************//**
 * Micron 1Gb MT25QL01GBBB SPI Flash Driver for Microchip PolarFire SoC (MPFS)
 * Modular Driver: Supports both FPGA CoreSPI and Hard MSS_SPI
 *******************************************************************************/

#ifndef MICRON1GFLASH_H_
#define MICRON1GFLASH_H_

#include <stdint.h>
#include <stddef.h>

/* Select Active SPI Driver Hardware Engine */
#if !defined(USE_CORE_SPI) && !defined(USE_MSS_SPI)
#define USE_CORE_SPI    /* Default to CoreSPI FPGA IP */
//#define USE_MSS_SPI   /* Uncomment to use Hard MSS SPI0 */
#endif

#if defined(USE_CORE_SPI)
#include "drivers/fpga_ip/CoreSPI/core_spi.h"
#ifndef CORESPI_BASE_ADDR
#define CORESPI_BASE_ADDR   0x40000000UL  /* Default FPGA CoreSPI Base Address */
#endif
#else
#include "drivers/mss/mss_spi/mss_spi.h"
#endif

#define DIE_ERASE_0_256MB     0
#define DIE_ERASE_256MB_512MB 1
#define DIE_ERASE_512MB_768MB 2
#define DIE_ERASE_768MB_1GB   3

#define ERASE_4K_BLOCK        0
#define ERASE_64K_BLOCK       1

#ifdef __cplusplus
extern "C" {
#endif

void FLASH_init(void);
void FLASH_read_device_id(uint8_t *manufacturer_id, uint8_t *device_id);
void FLASH_read(uint32_t address, uint8_t *rx_buffer, size_t size_in_bytes);
void FLASH_program(uint32_t address, uint8_t *write_buffer, size_t size_in_bytes);
void FLASH_erase_64k_block(uint32_t address);
void FLASH_global_unprotect(void);
void FLASH_die_256MB_erase(uint8_t die_number);

#ifdef __cplusplus
}
#endif

#endif /* MICRON1GFLASH_H_ */