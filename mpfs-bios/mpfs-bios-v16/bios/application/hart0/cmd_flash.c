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

/**
 * @brief Command 'flash_read'
 * Usage: flash_read <offset> [length]
 * Displays Flash contents formatted in hexadecimal on the console.
 */
/**
 * @brief Command 'flash_read'
 * Usage: flash_read <offset> [length]
 * Displays Flash contents formatted in hexadecimal on the console.
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
    printf("Reading %lu bytes from Flash Address 0x%08X:\n", requested_len, flash_offset);

    size_t bytes_remaining = requested_len;
    uint32_t current_addr = flash_offset;

    while (bytes_remaining > 0) {
        /* Determine chunk size for current iteration */
        size_t chunk_size = (bytes_remaining > sizeof(chunk_buf)) ? sizeof(chunk_buf) : bytes_remaining;

        if (spi_flash_read(current_addr, chunk_buf, chunk_size) != 0) {
            printf("\nError: Flash read failed at address 0x%08X!\n", current_addr);
            return;
        }

        /* Print hex dump */
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
 * @brief Command 'flash_copy'
 * Usage: flash_copy <offset> <ram_addr> [count]
 * Copies raw bytes from Flash memory into target RAM (LIM or DDR).
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
    printf("Copying %lu bytes from Flash 0x%08X to RAM 0x%016lx (%s)...\n",
           count, flash_offset, (unsigned long)ram_addr, spi_flash_get_driver_name());

    if (spi_flash_read(flash_offset, (uint8_t *)ram_addr, count) == 0) {
        printf("Flash copy complete.\n");
    } else {
        printf("Error: Flash copy failed!\n");
    }
}

/**
 * @brief Command 'flash_write'
 * Usage: flash_write <offset> <ram_addr> [count]
 * Writes data from RAM to Flash memory.
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
    printf("Writing %lu bytes from RAM 0x%016lx to Flash 0x%08X (%s)...\n",
           count, (unsigned long)ram_addr, flash_offset, spi_flash_get_driver_name());

    if (spi_flash_write(flash_offset, (const uint8_t *)ram_addr, count) == 0) {
        printf("Flash write complete.\n");
    } else {
        printf("Error: Flash write failed!\n");
    }
}

/**
 * @brief Command 'flash_erase_range'
 * Usage: flash_erase_range <offset> <count>
 * Erases a specified byte range in Flash memory.
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
    printf("Erasing Flash region 0x%08X (%lu bytes)...\n", offset, count);
    spi_flash_erase(offset, count);
}

/**
 * @brief Command 'spiflashboot'
 * Usage: spiflashboot [flash_offset] [ram_addr] [size]
 * Loads image from Flash to RAM and boots multi-hart execution.
 */
void spiflashboot_handler(int nb_params, char **params)
{
    uint32_t flash_addr = 0x00100000;
    uintptr_t ram_addr  = 0x08040000;
    size_t size         = 0x00040000;
    size_t max_ram_size = 0;
    uint8_t mfr_id = 0, dev_id = 0;

    if (nb_params >= 1) {
        flash_addr = (uint32_t)strtoul(params[0], NULL, 0);
    }
    if (nb_params >= 2) {
        ram_addr = (uintptr_t)strtoull(params[1], NULL, 0);
    }
    if (nb_params >= 3) {
        size = (size_t)strtoul(params[2], NULL, 0);
    }

    printf("\n==================================================\n");
    printf("         SPI Flash Boot Procedure                 \n");
    printf("==================================================\n");

    if (!boot_load_max_size((unsigned long)ram_addr, &max_ram_size)) {
        printf("Error: Destination RAM address 0x%016lx is invalid!\n", (unsigned long)ram_addr);
        return;
    }

    if (size > max_ram_size) {
        printf("Error: Requested size (%lu bytes) exceeds RAM capacity (%lu bytes)!\n", size, max_ram_size);
        return;
    }

    spi_flash_init();
    spi_flash_read_id(&mfr_id, &dev_id);
    printf("Active Driver : %s\n", spi_flash_get_driver_name());
    printf("Flash Hardware: Mfr=0x%02X, Dev=0x%02X\n", mfr_id, dev_id);

    printf("Reading %zu bytes from Flash 0x%08X into RAM 0x%016lx...\n",
           size, flash_addr, (unsigned long)ram_addr);

    if (spi_flash_read(flash_addr, (uint8_t *)ram_addr, size) == 0) {
        printf("SPI Flash transfer complete. Launching target application...\n");
        boot(0, 0, 0, (unsigned long)ram_addr);
    } else {
        printf("Error: SPI Flash read operation failed!\n");
    }
}