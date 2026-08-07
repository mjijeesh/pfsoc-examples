/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : e51.c
 * Description: Primary entry point, autoboot sequence, and BIOS shell loop on E51 Hart 0.
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mpfs_hal/mss_hal.h"
#include "uart.h"
#include "readline.h"
#include "command.h"
#include "boot.h"
#include "hw_info.h"

#define CPU_FREQ_HZ 600000000UL /* E51 Core Clock frequency (600 MHz) */

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
 * @brief Parses space-delimited shell command arguments.
 * @param buf Command line string.
 * @param command Pointer to extract command keyword.
 * @param params Array to store argument string pointers.
 * @return Number of parameters parsed.
 */
static int get_param(char *buf, char **command, char **params) {
    int nb_params = 0;
    *command = strtok(buf, " ");

    while (*command != NULL && nb_params < MAX_PARAM) {
        char *token = strtok(NULL, " ");
        if (token == NULL) break;
        params[nb_params++] = token;
    }
    return nb_params;
}

/**
 * @brief Displays a countdown timer allowing the user to abort autoboot.
 * @param timeout_sec Number of seconds to wait.
 * @return 1 if countdown completed (proceed to boot), 0 if interrupted by key press.
 */
static int autoboot_countdown(int timeout_sec)
{
    printf("Press any key to stop autoboot: %2d", timeout_sec);
    fflush(stdout);

    uint64_t start_cycles = get_cycles();
    uint64_t one_sec_cycles = CPU_FREQ_HZ;
    int remaining = timeout_sec;

    while (remaining > 0) {
        /* Check if user pressed any key on console */
        if (uart_read_nonblock()) {
            (void)uart_read(); /* Clear key from UART RX buffer */
            printf("\nAutoboot aborted by user.\n\n");
            return 0; /* Abort autoboot, return to shell */
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
    return 1; /* Proceed to autoboot */
}

/**
 * @brief Executes LiteX-style prioritized boot sequence:
 *        1. Try Serialboot (SFL host connection)
 *        2. Try Flashboot (.fbi image from SPI Flash offset 0x20000)
 */
static void boot_sequence(void)
{
    /* --- Boot Method 1: Serialboot (SFL over UART) --- */
    printf("--- Attempting Boot Method 1: Serialboot (SFL) ---\n");
    int status = serialboot();
    if (status == 0) {
        /* Boot completed or user handled serial action */
        return;
    }

    /* --- Boot Method 2: Flashboot (SPI Flash .fbi) --- */
    printf("\n--- Attempting Boot Method 2: Flashboot (.fbi) ---\n");
    flashboot_handler(0, NULL);
}

/**
 * @brief Primary BareMetal BIOS application entry point for Hart 0 (E51).
 */
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    char buffer[CMD_LINE_BUFFER_SIZE];
    char *params[MAX_PARAM];
    char *command;
    struct command_struct *cmd;
    int nb_params;

    /* 1. Initialize MMUART0 Driver Wrapper */
    uart_init();

    /* 2. Initialize Command History Buffer */
    hist_init();

    /* 3. Print PolarFire SoC BIOS Banner */
    printf("\n");
    printf("  ____  ___  _     _     ____  _____ ___ ____  _____   ____   ___   ____ \n");
    printf(" |  _ \\/ _ \\| |   / \\   |  _ \\|  ___|_ _|  _ \\| ____| / ___| / _ \\ / ___|\n");
    printf(" | |_) | | | | |  / _ \\  | |_) | |_   | || |_) |  _|   \\___ \\| | | | |    \n");
    printf(" |  __/| |_| | |_/ ___ \\ |  _ <|  _|  | ||  _ <| |___   ___) | |_| | |___\n");
    printf(" |_|    \\___/|____/_/   \\_\\|_| \\_\\|_|   |___|_| \\_____| |____/ \\___/ \\____|\n");
    printf("\n");
    printf("  Microchip PolarFire SoC BareMetal BIOS\n");
    printf("  --------------------------------------------------\n");
    printf("  Hart0E51MonitorCore      : Active\n");
    printf("  Harts1To4U54Application  : Parked In WFI\n");
    printf("  Memory Map               : LIM(0x08000000) | DDR(0x80000000)\n");
    printf("  Build Date & Time        : " __DATE__ " " __TIME__ "\n\n");
    fflush(stdout);

    /* 4. Clear pending software interrupts */
    clear_soft_interrupt();

    /* 5. Display Hardware Description (Header-Driven) */
    show_hw_info();

    /* 6. Autoboot Countdown & Sequence Execution */
    if (autoboot_countdown(10)) {
        boot_sequence();
    }

    /* 7. Interactive Console Loop */
    printf(PROMPT);
    fflush(stdout);

    while (1) {
        if (readline(buffer, CMD_LINE_BUFFER_SIZE) > 0) {
            printf("\n");
            nb_params = get_param(buffer, &command, params);
            if (command && *command != 0) {
                cmd = command_dispatcher(command, nb_params, params);
                if (!cmd) {
                    printf("Command '%s' not found. Type 'help'.\n", command);
                }
            }
            fflush(stdout);
        } else {
            printf("\n");
        }
        printf(PROMPT);
        fflush(stdout);
    }

    return 0;
}

void e51(void) {
    main(0, NULL);
}

/**
 * @brief Hart 0 local software interrupt service handler.
 */
void Software_h0_IRQHandler(void) {
    clear_soft_interrupt();
}