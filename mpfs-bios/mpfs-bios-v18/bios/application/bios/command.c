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




/* Primary command lookup table */
static const struct command_struct command_table[] = {
    // System Commands
    { help_handler,        "help",       "Print available commands",            SYSTEM_CMDS },
    { ident_handler,       "ident",      "Identifier of the system",           SYSTEM_CMDS },
    { hw_info_handler,     "hw_info",    "Display hardware map and peripherals", SYSTEM_CMDS },
    { reboot_handler,      "reboot",     "Reboot system",                       SYSTEM_CMDS },

    { sys_serial_handler,       "sys_serial",        "Read 128-bit Device Serial Number (DSN)",             SYSTEM_CMDS },
    { sys_info_handler,         "sys_info",          "Read FPGA Usercode and Design Information",            SYSTEM_CMDS },
    { sys_iap_handler,          "sys_iap",           "Execute IAP Bitstream Programming: sys_iap <addr>",   SYSTEM_CMDS },
    { sys_digest_handler,       "sys_digest",        "Run Digest Integrity Check across fabric & memories",   SYSTEM_CMDS },

    // Boot Commands
    { boot_handler,        "boot",       "Boot from Memory: boot <addr> [r1]",  BOOT_CMDS },
    { serialboot_handler,  "serialboot", "Boot from Serial (SFL)",              BOOT_CMDS },

    { flashwrite_handler,       "flashwrite",        "Upload binary to SPI Flash over SFL",                 BOOT_CMDS },
    { flashboot_handler,        "flashboot",         "Boot FBI image from SPI Flash: flashboot [offset] [ram_addr]", BOOT_CMDS },
    { flash_write_handler,      "flash_write",       "Write RAM to Flash: flash_write <offset> <ram_addr> [count]", BOOT_CMDS },
    { flash_erase_range_handler,"flash_erase_range", "Erase Flash range: flash_erase_range <offset> <count>",      BOOT_CMDS },
    { flash_read_handler,       "flash_read",        "Read Flash : flash_read <offset>  [count]",    BOOT_CMDS },
    { flash_copy_handler,       "flash_copy",        "Copy Flash to RAM: flash_copy <offset> <ram_addr> [count]",    BOOT_CMDS }, 
    


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
    printf("\n MPFS BIOS, available commands:\n\n");
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