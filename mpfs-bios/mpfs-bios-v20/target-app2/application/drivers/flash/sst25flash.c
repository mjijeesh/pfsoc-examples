/***************************************************************************//**
 * SST25 Series SPI Flash Driver Implementation for MPFS
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "sst25flash.h"
#include "spi_flash.h"
#include "mpfs_hal/mss_hal.h"

/* SST25 Command Opcodes */
#define SST25_READ_ARRAY_OPCODE     0x03
#define SST25_HIGH_SPEED_READ       0x0B
#define SST25_DEVICE_ID_READ        0x9F    /* JEDEC Read ID */
#define SST25_WRITE_ENABLE_CMD      0x06
#define SST25_WRITE_DISABLE_CMD     0x04
#define SST25_EWSR_CMD              0x50    /* Enable Write Status Register */
#define SST25_WRSR_CMD              0x01    /* Write Status Register */
#define SST25_READ_STATUS_CMD       0x05    /* Read Status Register */
#define SST25_BYTE_PAGE_PROGRAM     0x02    /* Page Program / Byte Program */
#define SST25_ERASE_4K_SECTOR       0x20    /* 4KB Sector Erase */
#define SST25_ERASE_32K_BLOCK       0x52    /* 32KB Block Erase */
#define SST25_ERASE_64K_BLOCK       0xD8    /* 64KB Block Erase */
#define SST25_CHIP_ERASE            0xC7    /* Chip Erase */

#define SST25_STATUS_BUSY_BIT       0x01    /* Bit 0 = Write / Erase in progress */

#if defined(USE_CORE_SPI)
static spi_instance_t g_sst_core_spi;
#define SPI_SLAVE                   0
#elif defined(USE_MSS_SPI1)
#define SPI_FLASH_PERIPH            MSS_PERIPH_SPI1
#define SPI_INSTANCE                (&g_mss_spi1_lo)
#define SPI_SLAVE                   MSS_SPI_SLAVE_0
#else
#define SPI_FLASH_PERIPH            MSS_PERIPH_SPI0
#define SPI_INSTANCE                (&g_mss_spi0_lo)
#define SPI_SLAVE                   MSS_SPI_SLAVE_0
#endif

/*******************************************************************************
 * Hardware Abstraction Layer (HAL) Helpers
 *******************************************************************************/
static void hal_spi_init(void)
{
#if defined(USE_CORE_SPI)
    SPI_init(&g_sst_core_spi, CORESPI_BASE_ADDR, 32);
    SPI_configure_master_mode(&g_sst_core_spi);
#else
    (void) mss_config_clk_rst(SPI_FLASH_PERIPH, (uint8_t)1, PERIPHERAL_ON);
    MSS_SPI_init(SPI_INSTANCE);
    MSS_SPI_configure_master_mode(
        SPI_INSTANCE,
        SPI_SLAVE,
        MSS_SPI_MODE0,
        8u,
        MSS_SPI_BLOCK_TRANSFER_FRAME_SIZE,
        NULL
    );
#endif
}

static void hal_spi_select(void)
{
#if defined(USE_CORE_SPI)
    SPI_set_slave_select(&g_sst_core_spi, SPI_SLAVE);
#else
    MSS_SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);
#endif
}

static void hal_spi_deselect(void)
{
#if defined(USE_CORE_SPI)
    SPI_clear_slave_select(&g_sst_core_spi, SPI_SLAVE);
#else
    MSS_SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);
#endif
}

static void hal_spi_transfer(const uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_len)
{
#if defined(USE_CORE_SPI)
    SPI_transfer_block(&g_sst_core_spi, (uint8_t *)tx, tx_len, rx, rx_len);
#else
    MSS_SPI_transfer_block(SPI_INSTANCE, (uint8_t *)tx, tx_len, rx, rx_len);
#endif
}

/*******************************************************************************
 * Private Helpers
 *******************************************************************************/
static void wait_until_ready(void)
{
    uint8_t cmd = SST25_READ_STATUS_CMD;
    uint8_t status = 0;

    do {
        hal_spi_select();
        hal_spi_transfer(&cmd, 1, &status, 1);
        hal_spi_deselect();
    } while (status & SST25_STATUS_BUSY_BIT);
}

static void write_enable(void)
{
    uint8_t cmd = SST25_WRITE_ENABLE_CMD;
    hal_spi_select();
    hal_spi_transfer(&cmd, 1, NULL, 0);
    hal_spi_deselect();
}

static void write_cmd_data(const uint8_t *cmd_buffer, uint16_t cmd_byte_size,
                            const uint8_t *data_buffer, uint16_t data_byte_size)
{
    uint8_t tx_buffer[520];
    uint16_t transfer_size = cmd_byte_size + data_byte_size;

    if (transfer_size > sizeof(tx_buffer)) return;

    memcpy(tx_buffer, cmd_buffer, cmd_byte_size);
    memcpy(&tx_buffer[cmd_byte_size], data_buffer, data_byte_size);

    hal_spi_transfer(tx_buffer, transfer_size, NULL, 0);
}

/*******************************************************************************
 * Public Driver API
 *******************************************************************************/
void SST25_FLASH_init(void)
{
    hal_spi_init();
    SST25_FLASH_global_unprotect();
}

void SST25_FLASH_read_device_id(uint8_t *manufacturer_id, uint8_t *device_id, uint8_t *capacity_id)
{
    uint8_t cmd = SST25_DEVICE_ID_READ;
    uint8_t rx_buf[3] = {0};

    hal_spi_select();
    hal_spi_transfer(&cmd, 1, rx_buf, sizeof(rx_buf));
    hal_spi_deselect();

    if (manufacturer_id) *manufacturer_id = rx_buf[0];
    if (device_id)       *device_id       = rx_buf[1];
    if (capacity_id)     *capacity_id     = rx_buf[2];
}

void SST25_FLASH_global_unprotect(void)
{
    uint8_t cmd;

    cmd = SST25_EWSR_CMD;
    hal_spi_select();
    hal_spi_transfer(&cmd, 1, NULL, 0);
    hal_spi_deselect();

    uint8_t wrsr_buf[2] = { SST25_WRSR_CMD, 0x00 };
    hal_spi_select();
    hal_spi_transfer(wrsr_buf, 2, NULL, 0);
    hal_spi_deselect();

    wait_until_ready();
}

void SST25_FLASH_read(uint32_t address, uint8_t *rx_buffer, size_t size_in_bytes)
{
    uint8_t cmd_buffer[4];

    cmd_buffer[0] = SST25_READ_ARRAY_OPCODE;
    cmd_buffer[1] = (uint8_t)((address >> 16) & 0xFF);
    cmd_buffer[2] = (uint8_t)((address >> 8) & 0xFF);
    cmd_buffer[3] = (uint8_t)(address & 0xFF);

    hal_spi_select();
    hal_spi_transfer(cmd_buffer, 4, rx_buffer, (uint16_t)size_in_bytes);
    hal_spi_deselect();
}

void SST25_FLASH_erase_4k_sector(uint32_t address)
{
    uint8_t cmd_buffer[4];

    write_enable();

    cmd_buffer[0] = SST25_ERASE_4K_SECTOR;
    cmd_buffer[1] = (uint8_t)((address >> 16) & 0xFF);
    cmd_buffer[2] = (uint8_t)((address >> 8) & 0xFF);
    cmd_buffer[3] = (uint8_t)(address & 0xFF);

    hal_spi_select();
    hal_spi_transfer(cmd_buffer, 4, NULL, 0);
    hal_spi_deselect();

    wait_until_ready();
}

void SST25_FLASH_erase_64k_block(uint32_t address)
{
    uint8_t cmd_buffer[4];

    write_enable();

    cmd_buffer[0] = SST25_ERASE_64K_BLOCK;
    cmd_buffer[1] = (uint8_t)((address >> 16) & 0xFF);
    cmd_buffer[2] = (uint8_t)((address >> 8) & 0xFF);
    cmd_buffer[3] = (uint8_t)(address & 0xFF);

    hal_spi_select();
    hal_spi_transfer(cmd_buffer, 4, NULL, 0);
    hal_spi_deselect();

    wait_until_ready();
}

void SST25_FLASH_erase_range(uint32_t address, size_t count)
{
    if (count == 0) return;

    uint32_t start_addr = address;
    uint32_t end_addr   = address + (uint32_t)count;
    uint32_t block_addr = start_addr & ~(0xFFFFUL);

    while (block_addr < end_addr) {
        SST25_FLASH_erase_64k_block(block_addr);
        block_addr += 0x10000UL;
    }
}

void SST25_FLASH_program(uint32_t address, const uint8_t *write_buffer, size_t size_in_bytes)
{
    uint8_t cmd_buffer[4];
    uint32_t in_buffer_idx = 0;
    uint32_t target_addr = address;

    while (in_buffer_idx < size_in_bytes) {
        uint32_t bytes_to_write = 256 - (target_addr & 0xFF);
        uint32_t bytes_remaining = size_in_bytes - in_buffer_idx;

        if (bytes_remaining < bytes_to_write) {
            bytes_to_write = bytes_remaining;
        }

        write_enable();

        cmd_buffer[0] = SST25_BYTE_PAGE_PROGRAM;
        cmd_buffer[1] = (uint8_t)((target_addr >> 16) & 0xFF);
        cmd_buffer[2] = (uint8_t)((target_addr >> 8) & 0xFF);
        cmd_buffer[3] = (uint8_t)(target_addr & 0xFF);

        hal_spi_select();
        write_cmd_data(cmd_buffer, sizeof(cmd_buffer), &write_buffer[in_buffer_idx], (uint16_t)bytes_to_write);
        hal_spi_deselect();

        wait_until_ready();

        target_addr += bytes_to_write;
        in_buffer_idx += bytes_to_write;
    }
}

/*******************************************************************************
 * Driver Operations Export Table
 *******************************************************************************/
static int sst25_init_wrapper(void)
{
    SST25_FLASH_init();
    return 0;
}

static int sst25_read_id_wrapper(uint8_t *mfr_id, uint8_t *dev_id, uint8_t *capacity_id)
{
    SST25_FLASH_read_device_id(mfr_id, dev_id, capacity_id);
    return 0;
}

static int sst25_read_wrapper(uint32_t offset, uint8_t *buf, size_t len)
{
    SST25_FLASH_read(offset, buf, len);
    return 0;
}

static int sst25_write_wrapper(uint32_t offset, const uint8_t *buf, size_t len)
{
    SST25_FLASH_program(offset, buf, len);
    return 0;
}

static int sst25_erase_wrapper(uint32_t offset, size_t len)
{
    SST25_FLASH_erase_range(offset, len);
    return 0;
}

const struct spi_flash_ops sst25_flash_ops = {
    .name    = "SST SST25 Series Flash",
    .init    = sst25_init_wrapper,
    .read_id = sst25_read_id_wrapper,
    .read    = sst25_read_wrapper,
    .write   = sst25_write_wrapper,
    .erase   = sst25_erase_wrapper
};