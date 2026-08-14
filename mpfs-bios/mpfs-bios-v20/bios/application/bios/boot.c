/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : boot.c
 * Description: Core bootloader routines, multi-hart launching, and SFL serial receiver.
 *******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mpfs_hal/mss_hal.h"
#include "sfl.h"
#include "crc.h"
#include "boot.h"
#include "uart.h"
#include "spi_flash.h"

#define MAX_FAILURES 256            /* Maximum permitted transfer retries before aborting */
#define CPU_FREQ_HZ  600000000UL    /* E51 Core Clock frequency (600 MHz) */

#ifndef FLASH_BOOT_ADDRESS
#define FLASH_BOOT_ADDRESS 0x00100000UL /* Default 1MB SPI Flash Offset */
#endif

typedef void (*entry_func_t)(unsigned long r1, unsigned long r2, unsigned long r3);

/**
 * @brief Reads the 64-bit RISC-V Machine Cycle counter (`rdcycle`).
 * @return Current core cycle tick count.
 */
static inline uint64_t get_cycles(void)
{
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

/**
 * @brief Validates destination memory bounds on PolarFire SoC.
 * @param addr Destination load address.
 * @param max_size Pointer to store maximum permissible write size.
 * @return 1 if address is within valid RAM ranges, 0 if out of bounds.
 */
int boot_load_max_size(unsigned long addr, size_t *max_size)
{
    // LIM Scratchpad Memory Range (0x08000000 - 0x08200000)
    if (addr >= 0x08000000UL && addr < 0x08200000UL) {
        *max_size = 0x08200000UL - addr;
        return 1;
    }
    // DDR Cached Memory Range (0x80000000 - 0xC0000000)
    if (addr >= 0x80000000UL && addr < 0xC0000000UL) {
        *max_size = 0xC0000000UL - addr;
        return 1;
    }
    // DDR Non-Cached Memory Range (0xC0000000 - 0xE0000000)
    if (addr >= 0xC0000000UL && addr < 0xE0000000UL) {
        *max_size = 0xE0000000UL - addr;
        return 1;
    }

    printf("Error: Boot load address 0x%08lx is outside valid memory!\n", addr);
    return 0;
}



/* Add to application/boot/boot.c */

extern void flashboot_handler(int nb_params, char **params);

/**
 * @brief Displays a countdown timer allowing the user to interrupt autoboot.
 * @param timeout_sec Seconds to wait.
 * @return 1 if countdown completed, 0 if interrupted by key press.
 */
static int autoboot_countdown(int timeout_sec)
{
    printf("Press any key to stop autoboot: %2d", timeout_sec);
    fflush(stdout);

    uint64_t start_cycles = get_cycles();
    uint64_t one_sec_cycles = CPU_FREQ_HZ;
    int remaining = timeout_sec;

    while (remaining > 0) {
        if (uart_read_nonblock()) {
            (void)uart_read(); /* Clear key from UART RX buffer */
            printf("\nAutoboot aborted by user.\n\n");
            return 0;
        }

        uint64_t current_cycles = get_cycles();
        if ((current_cycles - start_cycles) >= one_sec_cycles) {
            start_cycles += one_sec_cycles;
            remaining--;
            printf("\b\b%2d", remaining);
            fflush(stdout);
        }
    }

    printf("\n\nStarting autoboot sequence...\n\n");
    return 1;
}

/**
 * @brief High-level entry point to execute the boot sequence if not aborted.
 * @param timeout_sec Autoboot timer duration in seconds.
 */
void autoboot_run(int timeout_sec)
{
    if (!autoboot_countdown(timeout_sec)) {
        return;
    }

    /* Boot Method 1: Serialboot (SFL over UART) */
    printf("--- Attempting Boot Method 1: Serialboot (SFL) ---\n");
    if (serialboot() == 0) {
        return;
    }

    /* Boot Method 2: Flashboot (SPI Flash .fbi image) */
    printf("\n--- Attempting Boot Method 2: Flashboot (.fbi) ---\n");
    flashboot_handler(0, NULL);
}



/**
 * @brief Legacy single-hart direct jump function.
 * @param r1 Parameter passed in RISC-V register a0.
 * @param r2 Parameter passed in RISC-V register a1.
 * @param r3 Parameter passed in RISC-V register a2.
 * @param addr Binary entry point address.
 */
void __attribute__((noreturn)) bootold(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr)
{
    printf("Executing application at 0x%016lx...\n\n", addr);
    uart_sync();

    // Disable interrupts before jumping to new binary
    __disable_irq();

    // 1. Data memory fence: flush all pending write buffers
    asm volatile("fence" ::: "memory");

    // 2. Instruction fence: flush and synchronize RISC-V instruction pipeline
    asm volatile("fence.i" ::: "memory");

    // Jump execution to loaded payload
    entry_func_t entry = (entry_func_t)(uintptr_t)addr;
    entry(r1, r2, r3);

    while (1);
}

/* Global jump target address shared across all RISC-V harts */
volatile uint64_t g_app_jump_addr = 0;

/**
 * @brief Multi-hart boot routine: Publishes target address and triggers U54 IPIs.
 * @param r1 Register parameter 1.
 * @param r2 Register parameter 2.
 * @param r3 Register parameter 3.
 * @param addr Payload entry memory address.
 */
void boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr)
{
    (void)r1; (void)r2; (void)r3;

    printf("Launching target application on U54 harts at 0x%016lx...\n", addr);
    printf("E51 Monitor Core remains active in BIOS.\n\n");
    uart_sync();

    // 1. Publish jump address for U54 application harts
    g_app_jump_addr = addr;
    asm volatile("fence" ::: "memory");

    // 2. Raise Inter-Processor Interrupts (IPI) to U54 cores (Harts 1 to 4)
    for (uint32_t hart_id = 1; hart_id <= 4; hart_id++) {
        raise_soft_interrupt(hart_id);
    }

    // 3. Hardware spin delay to allow U54 cores to unpark and jump
    for (volatile int i = 0; i < 200000; i++);

    asm volatile("fence.i" ::: "memory");

    // Hart 0 (E51) returns cleanly back to shell execution loop
}

enum {
    ACK_TIMEOUT,
    ACK_CANCELLED,
    ACK_OK
};

/**
 * @brief Polls UART for host terminal ACK magic sequence.
 * @return ACK status code (`ACK_OK`, `ACK_TIMEOUT`, or `ACK_CANCELLED`).
 */
static int check_ack(void)
{
    int recognized = 0;
    static const char str[SFL_MAGIC_LEN] = SFL_MAGIC_ACK;
    uint64_t timeout_cycles = (CPU_FREQ_HZ / 4); // 250ms timeout window
    uint64_t start_cycles = get_cycles();

    while ((get_cycles() - start_cycles) < timeout_cycles) {
        if (uart_read_nonblock()) {
            char c = uart_read();
            if ((c == 'Q') || (c == '\e')) {
                return ACK_CANCELLED;
            }
            if (c == str[recognized]) {
                recognized++;
                if (recognized == SFL_MAGIC_LEN) {
                    return ACK_OK;
                }
            } else {
                recognized = (c == str[0]) ? 1 : 0;
            }
        }
    }
    return ACK_TIMEOUT;
}

/**
 * @brief Helper to decode big-endian 32-bit integer from byte stream.
 * @param data Pointer to 4-byte buffer.
 * @return Decoded 32-bit unsigned integer.
 */
static uint32_t get_uint32(const unsigned char *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8)  |
            (uint32_t)data[3];
}

/**
 * @brief Increments failure counter and reports abort threshold status.
 * @param failures Pointer to error counter.
 * @return 1 if threshold exceeded, 0 otherwise.
 */
static int serialboot_fail(int *failures)
{
    (*failures)++;
    if (*failures >= MAX_FAILURES) {
        return 1;
    }
    return 0;
}

/**
 * @brief Executes Serial Framing Protocol (SFL) frame reception state machine.
 * Supports direct RAM loading and direct SPI Flash programming.
 * @return 0 on successful application boot/write, 1 on error or abort.
 */
int serialboot(void)
{
    struct sfl_frame frame;
    int failures = 0;
    static const char str[SFL_MAGIC_LEN + 1] = SFL_MAGIC_REQ;
    const char *c;
    int ack_status;

    /* SFL Stream State Tracking */
    int is_flash_mode = 0;
    uint32_t spi_flash_address = FLASH_BOOT_ADDRESS;
    uint32_t spi_flash_start_address = 0;
    uint32_t total_loaded_size = 0;

    printf("Waiting for serialboot stream from host...\n");
    printf("Press Q or ESC to abort.\n");

    // 1. Send SFL Magic Request sequence to host terminal (`litex-term`)
    c = str;
    while (*c) {
        uart_write(*c++);
    }

    // 2. Await host ACK handshake response
    ack_status = check_ack();
    if (ack_status == ACK_TIMEOUT) {
        printf("Timeout waiting for SFL host connection.\n");
        return 1;
    }
    if (ack_status == ACK_CANCELLED) {
        printf("Serial boot cancelled by user.\n");
        return 0;
    }

    // 3. Handshake confirmed; enter frame receive loop
    while (1) {
        int i = 0;
        int timeout = 1;
        uint64_t frame_timeout_cycles = (CPU_FREQ_HZ / 4); // 250ms per byte timeout
        uint64_t start_cycles = get_cycles();

        while ((get_cycles() - start_cycles) < frame_timeout_cycles) {
            if (uart_read_nonblock()) {
                unsigned char data = (unsigned char)uart_read();
                start_cycles = get_cycles(); // Re-arm byte timeout on receipt

                if (i == 0) frame.payload_length = data;
                if (i == 1) frame.crc[0] = data;
                if (i == 2) frame.crc[1] = data;
                if (i == 3) frame.cmd    = data;
                if (i >= 4) {
                    frame.payload[i - 4] = data;
                }

                if (i == (frame.payload_length + 4 - 1)) {
                    timeout = 0;
                    break;
                }
                i++;
            }
        }

        if (timeout) {
            uart_write(SFL_ACK_ERROR);
            if (serialboot_fail(&failures))
                return 1;
            continue;
        }

        // 4. Validate payload CRC16
        uint16_t received_crc = ((uint16_t)frame.crc[0] << 8) | (uint16_t)frame.crc[1];
        uint16_t computed_crc = crc16(&frame.cmd, frame.payload_length + 1);

        if (computed_crc != received_crc) {
            uart_write(SFL_ACK_CRCERROR);
            if (serialboot_fail(&failures))
                return 1;
            continue;
        }

        // 5. Process SFL Command Opcodes (STRICTLY NO printf DURING HANDSHAKE OR FRAMES)
        switch (frame.cmd) {
        case SFL_CMD_ABORT:
            failures = 0;
            uart_write(SFL_ACK_SUCCESS);
            return 1;

        case SFL_CMD_FLASH: {
            is_flash_mode = 1;
            spi_flash_address = FLASH_BOOT_ADDRESS;

            if (frame.payload_length >= 4) {
                uint32_t total_image_size = get_uint32(&frame.payload[0]);

                if (frame.payload_length >= 8) {
                    spi_flash_address = get_uint32(&frame.payload[4]);
                }
                spi_flash_start_address = spi_flash_address;

                /* Initialize hardware SPI Flash and erase required block range quietly */
                // spi_flash_init();  /* Already initialized at boot */
                if (spi_flash_erase(spi_flash_address, total_image_size) == 0) {
                    uart_write(SFL_ACK_SUCCESS);
                } else {
                    uart_write(SFL_ACK_ERROR);
                }
            } else {
                uart_write(SFL_ACK_ERROR);
            }
            break;
        }

        case SFL_CMD_LOAD: {
            if (frame.payload_length < 4) {
                uart_write(SFL_ACK_ERROR);
                if (serialboot_fail(&failures)) return 1;
                break;
            }

            failures = 0;
            uint32_t load_size = frame.payload_length - 4;

            if (is_flash_mode) {
                /* Stream SFL chunk directly to SPI Flash */
                if (spi_flash_write(spi_flash_address, &frame.payload[4], load_size) == 0) {
                    spi_flash_address += load_size;
                    total_loaded_size += load_size;
                    uart_write(SFL_ACK_SUCCESS);
                } else {
                    uart_write(SFL_ACK_ERROR);
                }
            } else {
                /* Direct RAM Download Path */
                char *load_addr = (char *)(uintptr_t)get_uint32(&frame.payload[0]);
                size_t max_size;

                if (!boot_load_max_size((unsigned long)(uintptr_t)load_addr, &max_size) ||
                    (load_size > max_size)) {
                    uart_write(SFL_ACK_ERROR);
                    if (serialboot_fail(&failures)) return 1;
                    break;
                }

                memcpy(load_addr, &frame.payload[4], load_size);
                total_loaded_size += load_size;
                uart_write(SFL_ACK_SUCCESS);
            }
            break;
        }

        case SFL_CMD_DONE: {
            uart_write(SFL_ACK_SUCCESS);
            printf("\n=======================================================\n");
            printf(" [FLASHWRITE] SPI Flash Direct Upload Complete!\n");
            printf(" Active Driver      : %s\n", spi_flash_get_driver_name());
            printf(" Target Flash Addr  : 0x%08X\n", (unsigned int)spi_flash_start_address);
            printf(" Total Written Size : %u bytes (0x%X)\n", 
                   (unsigned int)total_loaded_size, (unsigned int)total_loaded_size);
            printf("=======================================================\n\n");
            return 0;
        }

        case SFL_CMD_JUMP: {
            uart_write(SFL_ACK_SUCCESS);

            /* Return cleanly if we were in direct SPI Flash mode */
            if (is_flash_mode) {
                printf("\n[INFO] Direct SPI Flash programming finished. Returning to prompt.\n");
                is_flash_mode = 0;
                return 0;
            }

            uint32_t jump_addr = get_uint32(&frame.payload[0]);

            // Launch target application across U54 cores
            boot(0, 0, 0, (unsigned long)jump_addr);

            // Return cleanly back to CLI shell prompt
            return 0;
        }

        default:
            uart_write(SFL_ACK_UNKNOWN);
            if (serialboot_fail(&failures)) return 1;
            break;
        }
    }
    return 1;
}

/**
 * @brief SFL entry point for Flash mode.
 */
int flashwrite(void)
{
    return serialboot();
}