/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : spi_flash.c
 * Description: Top-Level Generic SPI Flash Manager with Auto-Detection & Error Handling.
 *******************************************************************************/

#include <stdio.h>
#include "spi_flash.h"

/* JEDEC Manufacturer ID Definitions */
#define MFR_ID_MICRON   0x20
#define MFR_ID_SST      0xBF

/* External declarations of chip-specific driver operation tables */
extern const struct spi_flash_ops micron1g_flash_ops;
extern const struct spi_flash_ops sst25_flash_ops;

/* Active driver pointer */
static const struct spi_flash_ops *active_driver = &sst25_flash_ops;

int spi_flash_init(void)
{
    uint8_t mfr_id = 0;
    uint8_t dev_id = 0;
    uint8_t capacity_id = 0;

    /* 1. Initialize the low-level SPI bus hardware (MSS SPI / CoreSPI) */
    if (sst25_flash_ops.init) {
        sst25_flash_ops.init();
    }

    /* 2. Read JEDEC Manufacturer and Device ID over SPI */
    if (sst25_flash_ops.read_id) {
        sst25_flash_ops.read_id(&mfr_id, &dev_id, &capacity_id);
    }

    /* 3. Check for missing/floating bus hardware (0x00 or 0xFF) */
    if (mfr_id == 0x00 || mfr_id == 0xFF) {
        printf("Error: No SPI Flash memory detected! (Mfr ID: 0x%02X)\n", mfr_id);
        active_driver = NULL;
        return -1;
    }

    /* 4. Match Manufacturer ID to bind the proper driver */
    switch (mfr_id) {
        case MFR_ID_MICRON:
            active_driver = &micron1g_flash_ops;
            if (active_driver->init) {
                active_driver->init();
            }
            return 0;

        case MFR_ID_SST:
            active_driver = &sst25_flash_ops;
            if (active_driver->init) {
                active_driver->init(); /* Runs SST25-specific global unprotect */
            }
            return 0;

        default:
            printf("Error: Unsupported SPI Flash Mfr ID: 0x%02X, Dev ID: 0x%02X, Capacity ID: 0x%02X\n",
                   mfr_id, dev_id, capacity_id);
            active_driver = NULL;
            return -1;
    }
}

int spi_flash_read_id(uint8_t *mfr_id, uint8_t *dev_id , uint8_t *capacity_id)
{
    if (active_driver && active_driver->read_id) {
        return active_driver->read_id(mfr_id, dev_id, capacity_id);
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

const char *spi_flash_get_driver_name(void)
{
    if (active_driver && active_driver->name) {
        return active_driver->name;
    }
    return "No Driver Loaded / Unbound";
}