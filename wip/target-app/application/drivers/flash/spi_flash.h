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
 * @brief Function pointer table implemented by chip-specific drivers.
 */
struct spi_flash_ops {
    const char *name;
    int (*init)(void);
    int (*read_id)(uint8_t *mfr_id, uint8_t *dev_id);
    int (*read)(uint32_t offset, uint8_t *buf, size_t len);
    int (*write)(uint32_t offset, const uint8_t *buf, size_t len);
    int (*erase)(uint32_t offset, size_t len);
};

#ifdef __cplusplus
extern "C" {
#endif

/* Top-Level Generic SPI Flash API (Used by Shell & Bootloader) */
int spi_flash_init(void);
int spi_flash_read_id(uint8_t *mfr_id, uint8_t *dev_id);
int spi_flash_read(uint32_t offset, uint8_t *buf, size_t len);
int spi_flash_write(uint32_t offset, const uint8_t *buf, size_t len);
int spi_flash_erase(uint32_t offset, size_t len);
const char *spi_flash_get_driver_name(void);

#ifdef __cplusplus
}
#endif

#endif /* SPI_FLASH_H_ */