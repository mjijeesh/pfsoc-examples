/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : e51.c
 * Description: Primary entry point and BIOS shell loop executing on E51 Hart 0.
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mpfs_hal/mss_hal.h"
#include "uart.h"
#include "readline.h"
#include "command.h"
#include "serialboot.h"
#include "hw_info.h"

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
    printf("  Memory Map               : LIM(0x08000000) | DDR(0x80000000)\n\n");
    fflush(stdout);

    /* 4. Clear pending software interrupts */
    clear_soft_interrupt();

    /* 5. Display Hardware Description (Header-Driven) */
    show_hw_info();

    /* 6. Interactive Console Loop */
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