/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : spi_flash.c
 * Description: Top-Level Generic SPI Flash Manager.
 *******************************************************************************/

#include <stdio.h>
#include "spi_flash.h"

/* External declaration of driver ops defined in chip-specific drivers */
extern const struct spi_flash_ops micron1g_flash_ops;

/* Active driver pointer (Can be switched dynamically at runtime if needed) */
static const struct spi_flash_ops *active_driver = &micron1g_flash_ops;

int spi_flash_init(void)
{
    if (active_driver && active_driver->init) {
        return active_driver->init();
    }
    return -1;
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

const char *spi_flash_get_driver_name(void)
{
    if (active_driver && active_driver->name) {
        return active_driver->name;
    }
    return "Unknown Driver";
}