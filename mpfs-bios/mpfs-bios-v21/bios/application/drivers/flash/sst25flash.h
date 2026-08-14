/***************************************************************************//**
 * SST25 Series SPI Flash Driver for Microchip PolarFire SoC (MPFS)
 *******************************************************************************/

#ifndef SST25FLASH_H_
#define SST25FLASH_H_

#include <stdint.h>
#include <stddef.h>

#include "spi_flash.h"
#include "board_config.h"  /* Pulled automatically from boards/$(BOARD)/board_config.h */

#if defined(FLASH_USE_CORE_SPI)
#include "drivers/fpga_ip/CoreSPI/core_spi.h"
#ifndef CORESPI_BASE_ADDR
#define CORESPI_BASE_ADDR   0x40000000UL
#endif
#else
#include "drivers/mss/mss_spi/mss_spi.h"
#endif

#define SST25_MFR_ID        0xBF

#ifdef __cplusplus
extern "C" {
#endif

void SST25_FLASH_init(void);
void SST25_FLASH_read_device_id(uint8_t *manufacturer_id, uint8_t *device_id ,uint8_t *capacity_id);
void SST25_FLASH_read(uint32_t address, uint8_t *rx_buffer, size_t size_in_bytes);
void SST25_FLASH_program(uint32_t address, const uint8_t *write_buffer, size_t size_in_bytes);
void SST25_FLASH_erase_4k_sector(uint32_t address);
void SST25_FLASH_erase_64k_block(uint32_t address);
void SST25_FLASH_erase_range(uint32_t address, size_t count);
void SST25_FLASH_global_unprotect(void);

extern const struct spi_flash_ops sst25_flash_ops;

#ifdef __cplusplus
}
#endif

#endif /* SST25FLASH_H_ */