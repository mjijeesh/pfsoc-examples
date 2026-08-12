/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : spi_flash.h
 * Description: Generic SPI Flash Abstraction Layer API & Driver Operations Table.
 *******************************************************************************/

#ifndef SPI_FLASH_H_
#define SPI_FLASH_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @struct spi_flash_ops
 * @brief Complete function pointer table implemented by chip-specific drivers.
 */
struct spi_flash_ops {
    const char *name;
    int (*init)(void);
    int (*read_id)(uint8_t *mfr_id, uint8_t *dev_id);
    int (*read)(uint32_t offset, uint8_t *buf, size_t len);
    int (*write)(uint32_t offset, const uint8_t *buf, size_t len);
    int (*erase)(uint32_t offset, size_t len);
    int (*chip_erase)(void);
    int (*erase_4k)(uint32_t offset);
    int (*erase_64k)(uint32_t offset);
};

#ifdef __cplusplus
extern "C" {
#endif

/* Top-Level Generic SPI Flash API */
int spi_flash_init(void);
int spi_flash_read_id(uint8_t *mfr_id, uint8_t *dev_id);
int spi_flash_read(uint32_t offset, uint8_t *buf, size_t len);
int spi_flash_write(uint32_t offset, const uint8_t *buf, size_t len);
int spi_flash_erase(uint32_t offset, size_t len);
int spi_flash_chip_erase(void);
int spi_flash_erase_4k_sector(uint32_t offset);
int spi_flash_erase_64k_block(uint32_t offset);
const char *spi_flash_get_driver_name(void);

#ifdef __cplusplus
}
#endif

#endif /* SPI_FLASH_H_ */