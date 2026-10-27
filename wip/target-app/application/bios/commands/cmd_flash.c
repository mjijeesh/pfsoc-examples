/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : cmd_flash.c
 * Description: Generic CLI Commands for SPI Flash Operations.
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "spi_flash.h"
#include "command.h"
#include "boot.h"
#include "crc.h"

/**
 * @brief Command 'flashboot'
 * Usage: flashboot [flash_offset] [ram_addr]
 * Reads an .fbi image (8-byte header: 4-byte size + 4-byte CRC32) from SPI Flash,
 * copies payload to RAM (DDR or LIM), verifies CRC32 integrity, and executes.
 */
void flashboot_handler(int nb_params, char **params)
{
    uint32_t flash_offset = 0x0020000;  /* Default 1MB Flash Offset */
    uintptr_t ram_addr  = 0x80000000;   /* Default DDR (0x80000000) or LIM (0x08040000) */
    size_t max_ram_size = 0;
    uint8_t header[8];
    uint8_t mfr_id = 0, dev_id = 0;

    if (nb_params >= 1) {
        flash_offset = (uint32_t)strtoul(params[0], NULL, 0);
    }
    if (nb_params >= 2) {
        ram_addr = (uintptr_t)strtoull(params[1], NULL, 0);
    }

    printf("\n==================================================\n");
    printf("         SPI Flash FBI Boot Procedure            \n");
    printf("==================================================\n");

    /* 1. Initialize Driver & Query SPI Flash hardware */
    spi_flash_init();
    spi_flash_read_id(&mfr_id, &dev_id);
    printf("Active Driver : %s\n", spi_flash_get_driver_name());
    printf("Flash Hardware: Mfr=0x%02X, Dev=0x%02X\n", mfr_id, dev_id);

    /* 2. Read 8-byte FBI Header from Flash */
    printf("Reading FBI Header from Flash Offset 0x%08X...\n", flash_offset);
    if (spi_flash_read(flash_offset, header, 8) != 0) {
        printf("Error: Failed to read FBI header from Flash!\n");
        return;
    }

    /* FBI Header Decode (Little-Endian):
     * Bytes [0..3] : Image Size (uint32_t)
     * Bytes [4..7] : Expected CRC32 Checksum (uint32_t)
     */
    uint32_t image_size = ((uint32_t)header[0])       |
                          ((uint32_t)header[1] << 8)  |
                          ((uint32_t)header[2] << 16) |
                          ((uint32_t)header[3] << 24);

    uint32_t expected_crc = ((uint32_t)header[4])       |
                            ((uint32_t)header[5] << 8)  |
                            ((uint32_t)header[6] << 16) |
                            ((uint32_t)header[7] << 24);

    if (image_size == 0 || image_size == 0xFFFFFFFF) {
        printf("Error: Invalid FBI image size (0x%08X) at Flash 0x%08X! Is flash erased?\n",
               image_size, flash_offset);
        return;
    }

    printf("FBI Header Information:\n");
    printf("  - Image Size   : %u bytes (0x%X)\n", image_size, image_size);
    printf("  - Expected CRC : 0x%08X\n", expected_crc);
    printf("  - Target RAM   : 0x%016lx\n", (unsigned long)ram_addr);

    /* 3. Validate Target Memory Bounds (LIM/DDR) */
    if (!boot_load_max_size((unsigned long)ram_addr, &max_ram_size)) {
        printf("Error: Destination RAM address 0x%016lx is out of bounds!\n", (unsigned long)ram_addr);
        return;
    }

    if (image_size > max_ram_size) {
        printf("Error: Image size (%u bytes) exceeds RAM region capacity (%zu bytes)!\n",
               image_size, max_ram_size);
        return;
    }

    /* 4. Copy Payload from Flash (Offset + 8) into RAM */
    printf("Copying %u bytes from Flash 0x%08X to RAM 0x%016lx...\n",
           image_size, flash_offset + 8, (unsigned long)ram_addr);

    if (spi_flash_read(flash_offset + 8, (uint8_t *)ram_addr, image_size) != 0) {
        printf("Error: Failed to copy payload from Flash into RAM!\n");
        return;
    }

    /* 5. Compute CRC32 Checksum over RAM Payload */
    uint32_t calculated_crc = crc32((const uint8_t *)ram_addr, image_size);
    printf("Calculated CRC32 : 0x%08X\n", calculated_crc);

    if (calculated_crc != expected_crc) {
        printf("Error: CRC32 mismatch! Header=0x%08X, Computed=0x%08X\n",
               expected_crc, calculated_crc);
        return;
    }

    printf("CRC32 Verification PASSED!\n");
    printf("Launching target application across U54 cores...\n\n");

    /* 6. Jump to Application Execution */
    boot(0, 0, 0, (unsigned long)ram_addr);
}

/**
 * @brief Command 'flash_write'
 * Usage: flash_write <offset> <ram_addr> [count]
 */
void flash_write_handler(int nb_params, char **params)
{
    char *c;
    uint32_t flash_offset;
    uintptr_t ram_addr;
    size_t count = 1;

    if (nb_params < 2) {
        printf("Usage: flash_write <offset> <ram_addr> [count]\n");
        return;
    }

    flash_offset = (uint32_t)strtoul(params[0], &c, 0);
    if (*c != 0) {
        printf("Error: invalid offset\n");
        return;
    }

    ram_addr = (uintptr_t)strtoull(params[1], &c, 0);
    if (*c != 0) {
        printf("Error: invalid ram_addr\n");
        return;
    }

    if (nb_params >= 3) {
        count = (size_t)strtoul(params[2], &c, 0);
        if (*c != 0) {
            printf("Error: invalid count\n");
            return;
        }
    }

    spi_flash_init();
    printf("Writing %zu bytes from RAM 0x%016lx to Flash 0x%08X (%s)...\n",
           count, (unsigned long)ram_addr, flash_offset, spi_flash_get_driver_name());

    if (spi_flash_write(flash_offset, (const uint8_t *)ram_addr, count) == 0) {
        printf("Flash write complete.\n");
    } else {
        printf("Error: Flash write failed!\n");
    }
}

/**
 * @brief Command 'flash_copy'
 * Usage: flash_copy <offset> <ram_addr> [count]
 */
void flash_copy_handler(int nb_params, char **params)
{
    char *c;
    uint32_t flash_offset;
    uintptr_t ram_addr;
    size_t count = 1;

    if (nb_params < 2) {
        printf("Usage: flash_copy <offset> <ram_addr> [count]\n");
        return;
    }

    flash_offset = (uint32_t)strtoul(params[0], &c, 0);
    if (*c != 0) {
        printf("Error: invalid offset\n");
        return;
    }

    ram_addr = (uintptr_t)strtoull(params[1], &c, 0);
    if (*c != 0) {
        printf("Error: invalid ram_addr\n");
        return;
    }

    if (nb_params >= 3) {
        count = (size_t)strtoul(params[2], &c, 0);
        if (*c != 0) {
            printf("Error: invalid count\n");
            return;
        }
    }

    spi_flash_init();
    printf("Copying %zu bytes from Flash 0x%08X to RAM 0x%016lx (%s)...\n",
           count, flash_offset, (unsigned long)ram_addr, spi_flash_get_driver_name());

    if (spi_flash_read(flash_offset, (uint8_t *)ram_addr, count) == 0) {
        printf("Flash copy complete.\n");
    } else {
        printf("Error: Flash copy failed!\n");
    }
}

/**
 * @brief Command 'flash_read'
 * Usage: flash_read <offset> [length]
 */
void flash_read_handler(int nb_params, char **params)
{
    char *c;
    uint32_t flash_offset = 0x00000000;
    size_t requested_len = 64;
    uint8_t chunk_buf[64];

    if (nb_params >= 1) {
        flash_offset = (uint32_t)strtoul(params[0], &c, 0);
        if (*c != 0) {
            printf("Error: invalid offset\n");
            return;
        }
    }

    if (nb_params >= 2) {
        requested_len = (size_t)strtoul(params[1], &c, 0);
        if (*c != 0) {
            printf("Error: invalid length\n");
            return;
        }
    }

    spi_flash_init();
    printf("Reading %zu bytes from Flash Address 0x%08X:\n", requested_len, flash_offset);

    size_t bytes_remaining = requested_len;
    uint32_t current_addr = flash_offset;

    while (bytes_remaining > 0) {
        size_t chunk_size = (bytes_remaining > sizeof(chunk_buf)) ? sizeof(chunk_buf) : bytes_remaining;

        if (spi_flash_read(current_addr, chunk_buf, chunk_size) != 0) {
            printf("\nError: Flash read failed at address 0x%08X!\n", current_addr);
            return;
        }

        for (size_t i = 0; i < chunk_size; i++) {
            if ((i % 16) == 0) {
                printf("\n0x%08X: ", (unsigned int)(current_addr + i));
            }
            printf("%02X ", chunk_buf[i]);
        }

        bytes_remaining -= chunk_size;
        current_addr += chunk_size;
    }
    printf("\n\n");
}

/**
 * @brief Command 'flash_erase_range'
 * Usage: flash_erase_range <offset> <count>
 */
void flash_erase_range_handler(int nb_params, char **params)
{
    char *c;
    uint32_t offset;
    size_t count;

    if (nb_params < 2) {
        printf("Usage: flash_erase_range <offset> <count>\n");
        return;
    }

    offset = (uint32_t)strtoul(params[0], &c, 0);
    if (*c != 0) {
        printf("Error: invalid offset\n");
        return;
    }

    count = (size_t)strtoul(params[1], &c, 0);
    if (*c != 0) {
        printf("Error: invalid count\n");
        return;
    }

    spi_flash_init();
    printf("Erasing Flash region 0x%08X (+%zu bytes)...\n", offset, count);
    spi_flash_erase(offset, count);
}

/**
 * @brief Command 'flash_idcode'
 * Usage: flash_idcode
 * Initializes the active driver and displays the JEDEC Manufacturer ID and Device ID.
 */
void flash_idcode_handler(int nb_params, char **params)
{
    (void)nb_params;
    (void)params;
    uint8_t mfr_id = 0;
    uint8_t dev_id = 0;

    if (spi_flash_init() != 0) {
        printf("Error: Failed to initialize SPI Flash subsystem!\n");
        return;
    }

    if (spi_flash_read_id(&mfr_id, &dev_id) == 0) {
        printf("Active SPI Driver: %s\n", spi_flash_get_driver_name());
        printf("SPI Flash JEDEC Identification:\n");
        printf("  - Manufacturer ID : 0x%02X\n", mfr_id);
        printf("  - Device ID       : 0x%02X\n", dev_id);
    } else {
        printf("Error: Failed to read JEDEC ID from SPI Flash!\n");
    }
}