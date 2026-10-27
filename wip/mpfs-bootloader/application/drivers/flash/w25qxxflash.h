/***************************************************************************//**
 * Winbond W25QXX Series SPI Flash Driver for Microchip PolarFire SoC (MPFS)
 * Modular Driver: Supports both FPGA CoreSPI and Hard MSS_SPI
 *******************************************************************************/

#ifndef W25QXXFLASH_H_
#define W25QXXFLASH_H_

#include <stdint.h>
#include <stddef.h>
#include "board_config.h"  /* Pulled automatically from boards/$(BOARD)/board_config.h */

/* Winbond Manufacturer JEDEC ID */
#define W25Q_MFR_ID_WINBOND    0xEF

#if defined(FLASH_USE_CORE_SPI)
#include "drivers/fpga_ip/CoreSPI/core_spi.h"
#ifndef FLASH_CORESPI_BASE_ADDR
#define FLASH_CORESPI_BASE_ADDR   0x40000000UL  /* Default FPGA CoreSPI Base Address */
#endif
#else
#include "drivers/mss/mss_spi/mss_spi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void W25QXX_FLASH_init(void);
void W25QXX_FLASH_read_device_id(uint8_t *manufacturer_id, uint8_t *device_id, uint8_t *capacity_id);
void W25QXX_FLASH_read(uint32_t address, uint8_t *rx_buffer, size_t size_in_bytes);
void W25QXX_FLASH_program(uint32_t address, const uint8_t *write_buffer, size_t size_in_bytes);
void W25QXX_FLASH_erase_4k_sector(uint32_t address);
void W25QXX_FLASH_erase_64k_block(uint32_t address);
void W25QXX_FLASH_erase_range(uint32_t address, size_t count);
void W25QXX_FLASH_chip_erase(void);
void W25QXX_FLASH_global_unprotect(void);

/* Exported Driver Operations Table for spi_flash.c abstraction layer */
extern const struct spi_flash_ops w25qxx_flash_ops;

#ifdef __cplusplus
}
#endif

#endif /* W25QXXFLASH_H_ */