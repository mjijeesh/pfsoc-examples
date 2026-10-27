/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : cli.c
 * Description: Command Line Interface implementation for interactive BIOS shell.
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cli.h"
#include "readline.h"
#include "uart.h"
#include "serialboot.h"
#include "board_config.h"

#define MAX_LINE_LEN 128  /* Maximum length for command input buffer */
#define MAX_ARGS     8    /* Maximum number of parsed command arguments */

typedef void (*entry_func_t)(void);



/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : cli.c
 * Description: Interactive command-line shell loop and argument tokenizer.
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "cli.h"
#include "readline.h"
#include "command.h"

/**
 * @brief Parses space-delimited shell command arguments.
 */
static int get_param(char *buf, char **command, char **params)
{
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
 * @brief Runs the continuous CLI read-eval-print loop (REPL).
 */
void cli_loop(void)
{
    char buffer[CMD_LINE_BUFFER_SIZE];
    char *params[MAX_PARAM];
    char *command;
    struct command_struct *cmd;
    int nb_params;

    printf(CLI_PROMPT);
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
        printf(CLI_PROMPT);
        fflush(stdout);
    }
}



/**
 * @struct command
 * @brief Simple CLI command entry table record.
 */
struct command {
    const char *name;                      /**< Command name string */
    void (*func)(int argc, char **argv);  /**< Command handler callback */
    const char *help;                      /**< Description string */
};

/* Forward Declarations for Internal Handlers */
static void cmd_help(int argc, char **argv);
static void cmd_ident(int argc, char **argv);
static void cmd_mread(int argc, char **argv);
static void cmd_mwrite(int argc, char **argv);
static void cmd_go(int argc, char **argv);
static void cmd_serialboot(int argc, char **argv);

/* Shell Command Lookup Table */
static const struct command command_table[] = {
    { "help",       cmd_help,       "Show available commands" },
    { "ident",      cmd_ident,      "Display system identification" },
    { "mr",         cmd_mread,      "Memory Read:  mr <hex_addr> [count]" },
    { "mw",         cmd_mwrite,     "Memory Write: mw <hex_addr> <hex_val>" },
    { "go",         cmd_go,         "Jump execution: go <hex_addr>" },
    { "serialboot", cmd_serialboot, "Serial Boot mode" },
};

#define NUM_COMMANDS (sizeof(command_table) / sizeof(struct command))

/**
 * @brief Shell command wrapper to initiate Serial Framing Protocol (SFL) boot.
 */
static void cmd_serialboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    serialboot();
}

/**
 * @brief Shell command to display help descriptions for all CLI commands.
 */
static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("\r\nAvailable Commands:\r\n");
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        printf("  %-10s - %s\r\n", command_table[i].name, command_table[i].help);
    }
    printf("\r\n");
}

/**
 * @brief Shell command to display target system identification.
 */
static void cmd_ident(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("\r\nBareMetal BIOS for Microchip PolarFire SoC MPFS)\r\n");
    printf("Target Core: E51 Monitor Core (Hart 0)\r\n");
    printf("Build Date : " __DATE__ " " __TIME__ "\r\n\r\n");
}

/**
 * @brief Shell command to read and display 32-bit memory words in hexadecimal.
 * Usage: mr <hex_addr> [count]
 */
static void cmd_mread(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: mr <hex_addr> [count]\r\n");
        return;
    }

    uintptr_t addr = (uintptr_t)strtoull(argv[1], NULL, 16);
    size_t count = (argc >= 3) ? (size_t)strtoul(argv[2], NULL, 10) : 1;

    volatile uint32_t *p = (volatile uint32_t *)addr;
    printf("\r\nReading 0x%zx dwords from 0x%016lx:\r\n", count, (unsigned long)addr);

    for (size_t i = 0; i < count; i++) {
        if (i % 4 == 0) {
            printf("\r\n0x%016lx: ", (unsigned long)(addr + (i * 4)));
        }
        printf("%08lx ", (unsigned long)p[i]);
    }
    printf("\r\n\r\n");
}

/**
 * @brief Shell command to write a 32-bit word value directly to a memory address.
 * Usage: mw <hex_addr> <hex_val>
 */
static void cmd_mwrite(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: mw <hex_addr> <hex_val>\r\n");
        return;
    }

    uintptr_t addr = (uintptr_t)strtoull(argv[1], NULL, 16);
    uint32_t val = (uint32_t)strtoul(argv[2], NULL, 16);

    volatile uint32_t *p = (volatile uint32_t *)addr;
    *p = val;
    printf("Wrote 0x%08lx to 0x%016lx\r\n", (unsigned long)val, (unsigned long)addr);
}

/**
 * @brief Shell command to jump CPU execution to a specified memory address.
 * Usage: go <hex_addr>
 */
static void cmd_go(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: go <hex_addr>\r\n");
        return;
    }

    uintptr_t addr = (uintptr_t)strtoull(argv[1], NULL, 16);
    printf("Flushing serial output and jumping to 0x%016lx...\r\n\r\n", (unsigned long)addr);

    uart_sync();

    entry_func_t entry = (entry_func_t)addr;
    entry();
}



#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

/**
 * @brief Prints initial CLI startup banner to console.
 */
void cli_init(void)
{
    printf("\r\n");
    printf("==================================================\r\n");
    printf("         BareMetal BIOS for PolarFire SoC             \r\n");
    printf(" Build Time : " BUILD_TIMESTAMP "\r\n");
    printf("==================================================\r\n");
    printf("Type 'help' for a list of available commands.\r\n\r\n");
    fflush(stdout);
}

/**
 * @brief Runs interactive prompt reading, tokenizing, and dispatching user inputs.
 */
void cli_run(void)
{
    char line[MAX_LINE_LEN];
    char *argv[MAX_ARGS];

    while (1) {
        // 1. Print prompt and IMMEDIATELY flush to UART
        printf("pfsoc-bios> ");
        fflush(stdout);

        // 2. Read user input
        int len = readline(line, sizeof(line));

        // 3. If line is empty (user pressed Enter), loop back to reprint prompt
        if (len <= 0) {
            continue;
        }

        // 4. Tokenize arguments
        int argc = 0;
        char *token = strtok(line, " ");

        while (token != NULL && argc < MAX_ARGS) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }

        // 5. Find and execute matching command
        if (argc > 0) {
            int found = 0;
            for (size_t i = 0; i < NUM_COMMANDS; i++) {
                if (strcmp(argv[0], command_table[i].name) == 0) {
                    command_table[i].func(argc, argv);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("Command '%s' not recognized. Type 'help'.\r\n", argv[0]);
            }
        }

        // 6. Flush command output before loop restarts
        fflush(stdout);
    }
}