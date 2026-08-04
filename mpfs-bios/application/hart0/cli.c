#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cli.h"
#include "readline.h"
#include "uart.h"
#include "xmodem.h"
#include "serialboot.h"

#define MAX_LINE_LEN 128
#define MAX_ARGS     8

typedef void (*entry_func_t)(void);

struct command {
    const char *name;
    void (*func)(int argc, char **argv);
    const char *help;
};

// Forward Declarations
static void cmd_help(int argc, char **argv);
static void cmd_ident(int argc, char **argv);
static void cmd_mread(int argc, char **argv);
static void cmd_mwrite(int argc, char **argv);
static void cmd_go(int argc, char **argv);
static void cmd_xmodem(int argc, char **argv);
static void cmd_serialboot(int argc, char **argv);

static const struct command command_table[] = {
    { "help",       cmd_help,       "Show available commands" },
    { "ident",      cmd_ident,      "Display system identification" },
    { "mr",         cmd_mread,      "Memory Read:  mr <hex_addr> [count]" },
    { "mw",         cmd_mwrite,     "Memory Write: mw <hex_addr> <hex_val>" },
    { "go",         cmd_go,         "Jump execution: go <hex_addr>" },
    { "xmodem",     cmd_xmodem,     "Upload binary via XMODEM: xmodem <hex_addr>" },
    { "serialboot", cmd_serialboot, "LiteX Serial Boot mode" },
};

#define NUM_COMMANDS (sizeof(command_table) / sizeof(struct command))

static void cmd_serialboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    serialboot();
}

static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("\r\nAvailable Commands:\r\n");
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        printf("  %-10s - %s\r\n", command_table[i].name, command_table[i].help);
    }
    printf("\r\n");
}

static void cmd_ident(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("\r\nLiteX BIOS (Ported to Microchip PolarFire SoC MPFS)\r\n");
    printf("Target Core: E51 Monitor Core (Hart 0)\r\n\r\n");
}

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

static void cmd_xmodem(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: xmodem <hex_addr>\r\n");
        return;
    }

    uintptr_t addr = (uintptr_t)strtoull(argv[1], NULL, 16);
    xmodem_receive((uint8_t *)addr);
}

void cli_init(void)
{
    printf("\r\n");
    printf("==================================================\r\n");
    printf("         LiteX BIOS for PolarFire SoC             \r\n");
    printf("==================================================\r\n");
    printf("Type 'help' for a list of available commands.\r\n\r\n");
    fflush(stdout); // Force banner output to UART
}

void cli_run(void)
{
    char line[MAX_LINE_LEN];
    char *argv[MAX_ARGS];

    while (1) {
        // 1. Print prompt and IMMEDIATELY flush to UART
        printf("litex> ");
        fflush(stdout);

        // 2. Read user input
        int len = readline(line, sizeof(line));

        // 3. If line is empty (user just pressed Enter), loop back to reprint prompt
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