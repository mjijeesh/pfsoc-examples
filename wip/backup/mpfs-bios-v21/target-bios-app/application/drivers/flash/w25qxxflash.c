/***************************************************************************//**
 * Winbond W25QXX Series SPI Flash Driver Implementation for MPFS
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "w25qxxflash.h"
#include "spi_flash.h"
#include "mpfs_hal/mss_hal.h"

/* Winbond W25QXX Command Opcodes */
#define W25Q_CMD_JEDEC_ID           0x9F    /* Read JEDEC ID (Mfr, Type, Capacity) */
#define W25Q_CMD_READ_DATA          0x03    /* Read Data Array */
#define W25Q_CMD_FAST_READ          0x0B    /* Fast Read Array */
#define W25Q_CMD_WRITE_ENABLE       0x06    /* Write Enable */
#define W25Q_CMD_WRITE_DISABLE      0x04    /* Write Disable */
#define W25Q_CMD_PAGE_PROGRAM       0x02    /* Page Program (Up to 256 Bytes) */
#define W25Q_CMD_SECTOR_ERASE_4K    0x20    /* 4KB Sector Erase */
#define W25Q_CMD_BLOCK_ERASE_32K    0x52    /* 32KB Block Erase */
#define W25Q_CMD_BLOCK_ERASE_64K    0xD8    /* 64KB Block Erase */
#define W25Q_CMD_CHIP_ERASE         0xC7    /* Chip Erase (or 0x60) */
#define W25Q_CMD_READ_STATUS1       0x05    /* Read Status Register 1 */
#define W25Q_CMD_WRITE_STATUS1      0x01    /* Write Status Register 1 */
#define W25Q_CMD_ENTER_4BYTE_MODE   0xB7    /* Enter 4-Byte Address Mode (>16MB) */
#define W25Q_CMD_EXIT_4BYTE_MODE    0xE9    /* Exit 4-Byte Address Mode */

/* Winbond Status Register 1 Bits */
#define W25Q_STATUS1_BUSY_BIT       0x01    /* Bit 0 = Erase/Write in Progress */
#define W25Q_STATUS1_WEL_BIT        0x02    /* Bit 1 = Write Enable Latch */

/* Configure SPI Hardware Instance based on board definitions */
#if defined(FLASH_USE_CORE_SPI)
static spi_instance_t g_flash_core_spi;
#define SPI_SLAVE                   FLASH_SPI_SLAVE_SELECT

#elif defined(FLASH_USE_MSS_SPI1)
#define SPI_FLASH_PERIPH            MSS_PERIPH_SPI1
#define SPI_INSTANCE                (&g_mss_spi1_lo)
#define SPI_SLAVE                   MSS_SPI_SLAVE_0

#else /* Default: FLASH_USE_MSS_SPI0 */
#define SPI_FLASH_PERIPH            MSS_PERIPH_SPI0
#define SPI_INSTANCE                (&g_mss_spi0_lo)
#define SPI_SLAVE                   MSS_SPI_SLAVE_0
#endif

/*******************************************************************************
 * Hardware Abstraction Layer (HAL) Helpers
 *******************************************************************************/
static void hal_spi_init(void)
{
#if defined(FLASH_USE_CORE_SPI)
    SPI_init(&g_flash_core_spi, FLASH_CORESPI_BASE_ADDR, 32);
    SPI_configure_master_mode(&g_flash_core_spi);
#else
    (void) mss_config_clk_rst(SPI_FLASH_PERIPH, (uint8_t)1, PERIPHERAL_ON);
    MSS_SPI_init(SPI_INSTANCE);
    MSS_SPI_configure_master_mode(
        SPI_INSTANCE,
        SPI_SLAVE,
        MSS_SPI_MODE0,
        8u,  /* 8-bit SPI transfers */
        MSS_SPI_BLOCK_TRANSFER_FRAME_SIZE,
        NULL
    );
#endif
}

static void hal_spi_select(void)
{
#if defined(FLASH_USE_CORE_SPI)
    SPI_set_slave_select(&g_flash_core_spi, SPI_SLAVE);
#else
    MSS_SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);
#endif
}

static void hal_spi_deselect(void)
{
#if defined(FLASH_USE_CORE_SPI)
    SPI_clear_slave_select(&g_flash_core_spi, SPI_SLAVE);
#else
    MSS_SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);
#endif
}

static void hal_spi_transfer(const uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_len)
{
#if defined(FLASH_USE_CORE_SPI)
    SPI_transfer_block(&g_flash_core_spi, (uint8_t *)tx, tx_len, rx, rx_len);
#else
    MSS_SPI_transfer_block(SPI_INSTANCE, (uint8_t *)tx, tx_len, rx, rx_len);
#endif
}

/*******************************************************************************
 * Private Helper Functions
 *******************************************************************************/

/**
 * @brief Polls Winbond Status Register 1 until BUSY bit (Bit 0) clears[cite: 31].
 */
static void wait_until_not_busy(void)
{
    uint8_t cmd = W25Q_CMD_READ_STATUS1;
    uint8_t status = 0;

    do {
        hal_spi_select();
        hal_spi_transfer(&cmd, 1, &status, 1);
        hal_spi_deselect();
    } while (status & W25Q_STATUS1_BUSY_BIT);
}

static void write_enable(void)
{
    uint8_t cmd_buffer = W25Q_CMD_WRITE_ENABLE;
    hal_spi_select();
    hal_spi_transfer(&cmd_buffer, 1, NULL, 0);
    hal_spi_deselect();
}

static void enter_4byte_address_mode(void)
{
#if defined(SPI_FLASH_ADDRESS_BYTES) && (SPI_FLASH_ADDRESS_BYTES == 4)
    uint8_t cmd_buffer = W25Q_CMD_ENTER_4BYTE_MODE;
    hal_spi_select();
    hal_spi_transfer(&cmd_buffer, 1, NULL, 0);
    hal_spi_deselect();
#endif
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

/**
 * @brief Constructs Command + Address header based on 3-byte vs 4-byte address mode.
 */
static uint16_t prepare_address_header(uint8_t opcode, uint32_t address, uint8_t *cmd_buf)
{
    cmd_buf[0] = opcode;

#if defined(SPI_FLASH_ADDRESS_BYTES) && (SPI_FLASH_ADDRESS_BYTES == 4)
    cmd_buf[1] = (uint8_t)((address >> 24) & 0xFF);
    cmd_buf[2] = (uint8_t)((address >> 16) & 0xFF);
    cmd_buf[3] = (uint8_t)((address >> 8) & 0xFF);
    cmd_buf[4] = (uint8_t)(address & 0xFF);
    return 5;
#else
    cmd_buf[1] = (uint8_t)((address >> 16) & 0xFF);
    cmd_buf[2] = (uint8_t)((address >> 8) & 0xFF);
    cmd_buf[3] = (uint8_t)(address & 0xFF);
    return 4;
#endif
}

/*******************************************************************************
 * Public Driver API
 *******************************************************************************/

void W25QXX_FLASH_init(void)
{
    hal_spi_init();
    enter_4byte_address_mode();
}

void W25QXX_FLASH_read_device_id(uint8_t *manufacturer_id, uint8_t *device_id, uint8_t *capacity_id)
{
    uint8_t read_device_id_cmd = W25Q_CMD_JEDEC_ID;
    uint8_t read_buffer[3] = {0};

    hal_spi_select();
    hal_spi_transfer(&read_device_id_cmd, 1, read_buffer, sizeof(read_buffer));
    hal_spi_deselect();

    if (manufacturer_id) *manufacturer_id = read_buffer[0];
    if (device_id)       *device_id       = read_buffer[1];
    if (capacity_id)     *capacity_id     = read_buffer[2];
}

void W25QXX_FLASH_read(uint32_t address, uint8_t *rx_buffer, size_t size_in_bytes)
{
    uint8_t cmd_buffer[5];
    uint16_t header_len;

    header_len = prepare_address_header(W25Q_CMD_READ_DATA, address, cmd_buffer);

    hal_spi_select();
    hal_spi_transfer(cmd_buffer, header_len, rx_buffer, (uint16_t)size_in_bytes);
    hal_spi_deselect();
}

void W25QXX_FLASH_global_unprotect(void)
{
    uint8_t cmd_buffer[2];
    write_enable();

    /* Clear BP0, BP1, BP2 block protection bits in Status Register 1 */
    cmd_buffer[0] = W25Q_CMD_WRITE_STATUS1;
    cmd_buffer[1] = 0x00;

    hal_spi_select();
    hal_spi_transfer(cmd_buffer, 2, NULL, 0);
    hal_spi_deselect();

    wait_until_not_busy();
}

void W25QXX_FLASH_erase_4k_sector(uint32_t address)
{
    uint8_t cmd_buffer[5];
    uint16_t header_len;

    write_enable();
    header_len = prepare_address_header(W25Q_CMD_SECTOR_ERASE_4K, address, cmd_buffer);

    hal_spi_select();
    hal_spi_transfer(cmd_buffer, header_len, NULL, 0);
    hal_spi_deselect();

    wait_until_not_busy();
}

void W25QXX_FLASH_erase_64k_block(uint32_t address)
{
    uint8_t cmd_buffer[5];
    uint16_t header_len;

    write_enable();
    header_len = prepare_address_header(W25Q_CMD_BLOCK_ERASE_64K, address, cmd_buffer);

    hal_spi_select();
    hal_spi_transfer(cmd_buffer, header_len, NULL, 0);
    hal_spi_deselect();

    wait_until_not_busy();
}

void W25QXX_FLASH_erase_range(uint32_t address, size_t count)
{
    if (count == 0) return;

    uint32_t start_addr = address;
    uint32_t end_addr   = address + (uint32_t)count;
    uint32_t block_addr = start_addr & ~(0xFFFFUL);

    while (block_addr < end_addr) {
        W25QXX_FLASH_erase_64k_block(block_addr);
        block_addr += 0x10000UL;
    }
}

void W25QXX_FLASH_chip_erase(void)
{
    uint8_t cmd_buffer = W25Q_CMD_CHIP_ERASE;

    write_enable();

    hal_spi_select();
    hal_spi_transfer(&cmd_buffer, 1, NULL, 0);
    hal_spi_deselect();

    wait_until_not_busy();
}

void W25QXX_FLASH_program(uint32_t address, const uint8_t *write_buffer, size_t size_in_bytes)
{
    uint8_t cmd_buffer[5];
    uint32_t in_buffer_idx = 0;
    uint32_t target_addr = address;

    while (in_buffer_idx < size_in_bytes)
    {
        /* Calculate remaining bytes to end of 256-byte page boundary */
        uint32_t bytes_to_write = 256 - (target_addr & 0xFF);
        uint32_t size_left = size_in_bytes - in_buffer_idx;

        if (size_left < bytes_to_write) {
            bytes_to_write = size_left;
        }

        write_enable();

        uint16_t header_len = prepare_address_header(W25Q_CMD_PAGE_PROGRAM, target_addr, cmd_buffer);

        hal_spi_select();
        write_cmd_data(cmd_buffer, header_len, &write_buffer[in_buffer_idx], (uint16_t)bytes_to_write);
        hal_spi_deselect();

        wait_until_not_busy();

        target_addr += bytes_to_write;
        in_buffer_idx += bytes_to_write;
    }

    uint8_t disable_cmd = W25Q_CMD_WRITE_DISABLE;
    hal_spi_select();
    hal_spi_transfer(&disable_cmd, 1, NULL, 0);
    hal_spi_deselect();
}

/*******************************************************************************
 * Driver Operations Export Table for spi_flash.c
 *******************************************************************************/
static int w25qxx_init_wrapper(void)
{
    W25QXX_FLASH_init();
    return 0;
}

static int w25qxx_read_id_wrapper(uint8_t *mfr_id, uint8_t *dev_id, uint8_t *capacity_id)
{
    W25QXX_FLASH_read_device_id(mfr_id, dev_id, capacity_id);
    return 0;
}

static int w25qxx_read_wrapper(uint32_t offset, uint8_t *buf, size_t len)
{
    W25QXX_FLASH_read(offset, buf, len);
    return 0;
}

static int w25qxx_write_wrapper(uint32_t offset, const uint8_t *buf, size_t len)
{
    W25QXX_FLASH_program(offset, buf, len);
    return 0;
}

static int w25qxx_erase_wrapper(uint32_t offset, size_t len)
{
    W25QXX_FLASH_erase_range(offset, len);
    return 0;
}

const struct spi_flash_ops w25qxx_flash_ops = {
    .name    = "Winbond W25QXX Series Flash",
    .init    = w25qxx_init_wrapper,
    .read_id = w25qxx_read_id_wrapper,
    .read    = w25qxx_read_wrapper,
    .write   = w25qxx_write_wrapper,
    .erase   = w25qxx_erase_wrapper
};