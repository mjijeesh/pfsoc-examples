/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : command.c
 * Description: Implementation of BIOS command table and dispatch handlers.
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "command.h"

/* External declarations for command handlers */
extern void help_handler(int nb_params, char **params);
extern void ident_handler(int nb_params, char **params);
extern void reboot_handler(int nb_params, char **params);

extern void boot_handler(int nb_params, char **params);
extern void serialboot_handler(int nb_params, char **params);

extern void mem_read_handler(int nb_params, char **params);
extern void mem_write_handler(int nb_params, char **params);
extern void mem_copy_handler(int nb_params, char **params);
extern void mem_test_handler(int nb_params, char **params);
extern void mem_cmp_handler(int nb_params, char **params);
extern void hw_info_handler(int nb_params, char **params);

/* Flash Commands */
extern void flash_test_handler(int nb_params, char **params);
extern void spiflashboot_handler(int nb_params, char **params);

/* Primary command lookup table */
static const struct command_struct command_table[] = {
    // System Commands
    { help_handler,        "help",       "Print available commands",            SYSTEM_CMDS },
    { ident_handler,       "ident",      "Identifier of the system",           SYSTEM_CMDS },
    { hw_info_handler,     "hw_info",    "Display hardware map and peripherals", SYSTEM_CMDS },
    { reboot_handler,      "reboot",     "Reboot system",                       SYSTEM_CMDS },

    // Boot Commands
    { boot_handler,        "boot",       "Boot from Memory: boot <addr> [r1]",  BOOT_CMDS },
    { serialboot_handler,  "serialboot", "Boot from Serial (SFL)",              BOOT_CMDS },
    { spiflashboot_handler,"spiflashboot", "Boot from SPI Flash: spiflashboot [flash_addr] [ram_addr] [len]", BOOT_CMDS },
    { flash_test_handler,  "flash_test",   "Test Flash ID & read: flash_test [flash_addr] [len]", BOOT_CMDS },

    // Memory Commands
    { mem_read_handler,    "mem_read",   "Read memory: mem_read <addr> [len]",  MEM_CMDS },
    { mem_write_handler,   "mem_write",  "Write memory: mem_write <addr> <val>", MEM_CMDS },
    { mem_copy_handler,    "mem_copy",   "Copy memory: mem_copy <dst> <src> [n]", MEM_CMDS },
    { mem_test_handler,    "mem_test",   "Test memory access: mem_test <addr>", MEM_CMDS },
    { mem_cmp_handler,     "mem_cmp",    "Compare memory: mem_cmp <a1> <a2> <n>", MEM_CMDS },
};

#define NUM_COMMANDS (sizeof(command_table) / sizeof(struct command_struct))

/**
 * @brief Scans command table and executes callback if command matches input.
 * @param command User input command keyword.
 * @param nb_params Argument count.
 * @param params Array of argument strings.
 * @return Executed command pointer, or NULL if command not found.
 */
struct command_struct *command_dispatcher(char *command, int nb_params, char **params)
{
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(command, command_table[i].name) == 0) {
            command_table[i].func(nb_params, params);
            return (struct command_struct *)&command_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Displays command help organized by functional group.
 */
void command_help_show(void)
{
    printf("\nLiteX BIOS, available commands:\n\n");
    const char *group_names[] = { "System Commands", "Boot Commands", "Memory Commands" };

    for (int g = 0; g < NB_OF_GROUPS; g++) {
        int printed_header = 0;
        for (size_t i = 0; i < NUM_COMMANDS; i++) {
            if (command_table[i].group == g) {
                if (!printed_header) {
                    printf("-- %s --\n", group_names[g]);
                    printed_header = 1;
                }
                printf("  %-16s - %s\n", command_table[i].name, command_table[i].help);
            }
        }
        if (printed_header) {
            printf("\n");
        }
    }
}