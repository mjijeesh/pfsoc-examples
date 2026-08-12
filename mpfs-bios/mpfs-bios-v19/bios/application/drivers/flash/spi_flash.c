/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : spi_flash.c
 * Description: Top-Level Generic SPI Flash Manager Configured via CFLAGS.
 *******************************************************************************/

#include <stdio.h>
#include "spi_flash.h"

/* External declarations of chip-specific driver operation tables */
extern const struct spi_flash_ops micron1g_flash_ops;
extern const struct spi_flash_ops sst25_flash_ops;

/* Bind active driver operations table at compile time based on Makefile CFLAGS */
#if defined(FLASH_MICRON_1G)
    #define BOARD_PRIMARY_FLASH_OPS    (&micron1g_flash_ops)
#elif defined(FLASH_SST25WF080B) || defined(FLASH_SST25PF040C) || defined(FLASH_SST25)
    #define BOARD_PRIMARY_FLASH_OPS    (&sst25_flash_ops)
#else
    #define BOARD_PRIMARY_FLASH_OPS    (&sst25_flash_ops)
#endif

static const struct spi_flash_ops *active_driver = BOARD_PRIMARY_FLASH_OPS;

int spi_flash_init(void)
{
    uint8_t mfr_id = 0;
    uint8_t dev_id = 0;

    if (!active_driver || !active_driver->init) {
        printf("[SPI FLASH] Error: No valid driver bound in configuration!\n");
        return -1;
    }

    if (active_driver->init() != 0) {
        printf("[SPI FLASH] Error: Failed to initialize SPI hardware driver!\n");
        return -1;
    }

    if (active_driver->read_id) {
        active_driver->read_id(&mfr_id, &dev_id);
    }

    if (mfr_id == 0x00 || mfr_id == 0xFF) {
        printf("[SPI FLASH] Error: No SPI Flash memory detected! (Mfr ID: 0x%02X)\n", mfr_id);
        active_driver = NULL;
        return -1;
    }

    printf("[SPI FLASH] Initialized: %s (Mfr: 0x%02X, Dev: 0x%02X)\n",
           active_driver->name, mfr_id, dev_id);

    return 0;
}

int spi_flash_read_id(uint8_t *mfr_id, uint8_t *dev_id)
{
    if (active_driver && active_driver->read_id) {
        return active_driver->read_id(mfr_id, dev_id);
    }
    return -1;
}

int spi_flash_read(uint32_t offset, uint8_t *buf, size_t len)
{
    if (active_driver && active_driver->read) {
        return active_driver->read(offset, buf, len);
    }
    return -1;
}

int spi_flash_write(uint32_t offset, const uint8_t *buf, size_t len)
{
    if (active_driver && active_driver->write) {
        return active_driver->write(offset, buf, len);
    }
    return -1;
}

int spi_flash_erase(uint32_t offset, size_t len)
{
    if (active_driver && active_driver->erase) {
        return active_driver->erase(offset, len);
    }
    return -1;
}

int spi_flash_chip_erase(void)
{
    if (active_driver && active_driver->chip_erase) {
        return active_driver->chip_erase();
    }
    return -1;
}

int spi_flash_erase_4k_sector(uint32_t offset)
{
    if (active_driver && active_driver->erase_4k) {
        return active_driver->erase_4k(offset);
    }
    return -1;
}

int spi_flash_erase_64k_block(uint32_t offset)
{
    if (active_driver && active_driver->erase_64k) {
        return active_driver->erase_64k(offset);
    }
    return -1;
}

const char *spi_flash_get_driver_name(void)
{
    if (active_driver && active_driver->name) {
        return active_driver->name;
    }
    return "No Driver Loaded / Unbound";
}