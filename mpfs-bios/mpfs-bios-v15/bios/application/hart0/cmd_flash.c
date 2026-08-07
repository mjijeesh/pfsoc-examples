/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : cmd_flash.c
 * Description: Command handlers for Micron SPI Flash inspection & boot.
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "micron1gflash.h"
#include "command.h"
#include "boot.h"

extern int boot_load_max_size(unsigned long addr, size_t *max_size);

/**
 * @brief Shell handler for 'flash_test' command.
 * Syntax: flash_test [flash_addr] [length]
 */
void flash_test_handler(int nb_params, char **params)
{
    uint8_t mfr_id = 0, dev_id = 0;
    uint32_t flash_addr = 0x00000000;
    size_t length = 32;
    uint8_t read_buf[64] = {0};

    if (nb_params >= 1) {
        flash_addr = (uint32_t)strtoul(params[0], NULL, 0);
    }
    if (nb_params >= 2) {
        length = (size_t)strtoul(params[1], NULL, 0);
        if (length > sizeof(read_buf)) {
            length = sizeof(read_buf);
        }
    }

    printf("\nInitializing Micron 1Gb SPI Flash Driver...\n");
    FLASH_init();

    FLASH_read_device_id(&mfr_id, &dev_id);
    printf("Micron Flash JEDEC ID -> Manufacturer: 0x%02X, Device: 0x%02X\n", mfr_id, dev_id);

    if (mfr_id == 0x00 || mfr_id == 0xFF) {
        printf("Error: SPI Flash communication failure or device missing!\n");
        return;
    }

    printf("Reading %zu bytes from Flash 0x%08X:\n", length, flash_addr);
    FLASH_read(flash_addr, read_buf, length);

    for (size_t i = 0; i < length; i++) {
        if (i % 16 == 0) {
            printf("\n0x%08X: ", (unsigned int)(flash_addr + i));
        }
        printf("%02X ", read_buf[i]);
    }
    printf("\n\n");
}

/**
 * @brief Shell handler for 'spiflashboot' command.
 * Syntax: spiflashboot [flash_addr] [ram_addr] [size_bytes]
 */
void spiflashboot_handler(int nb_params, char **params)
{
    uint32_t flash_addr = 0x00100000; /* Default 1MB Flash Offset */
    uintptr_t ram_addr  = 0x08040000; /* Default Upper L2-LIM RAM */
    size_t size         = 0x00040000; /* Default 256 KB */
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

    /* 1. Validate Memory Destination Bounds */
    if (!boot_load_max_size((unsigned long)ram_addr, &max_ram_size)) {
        printf("Error: Destination RAM address 0x%016lx is invalid!\n", (unsigned long)ram_addr);
        return;
    }

    if (size > max_ram_size) {
        printf("Error: Size (%zu bytes) exceeds destination RAM capacity (%zu bytes)!\n", size, max_ram_size);
        return;
    }

    /* 2. Initialize Driver & Verify ID */
    printf("Initializing SPI Hardware Driver...\n");
    FLASH_init();

    FLASH_read_device_id(&mfr_id, &dev_id);
    printf("Micron JEDEC ID : Mfr=0x%02X, Dev=0x%02X\n", mfr_id, dev_id);

    if (mfr_id == 0x00 || mfr_id == 0xFF) {
        printf("Error: SPI Flash communication failure!\n");
        return;
    }

    /* 3. Load payload from SPI Flash to RAM */
    printf("Reading %zu bytes from Flash 0x%08X into RAM 0x%016lx...\n",
           size, flash_addr, (unsigned long)ram_addr);

    FLASH_read(flash_addr, (uint8_t *)ram_addr, size);

    printf("SPI Flash transfer complete successfully!\n");

    /* 4. Launch Target Application on U54 Application Core */
    boot(0, 0, 0, (unsigned long)ram_addr);
}