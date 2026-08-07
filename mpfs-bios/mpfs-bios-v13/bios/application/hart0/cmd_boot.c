/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : cmd_boot.c
 * Description: Command handlers for boot and serialboot shell commands.
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "command.h"
#include "serialboot.h"

/**
 * @brief Shell handler for 'boot' command.
 * Usage: boot <address> [r1]
 */
void boot_handler(int nb_params, char **params)
{
    char *c;
    unsigned long addr;
    unsigned long r1 = 0;

    if (nb_params < 1) {
        printf("Usage: boot <address> [r1]\n");
        return;
    }

    addr = strtoul(params[0], &c, 0);
    if (nb_params > 1) {
        r1 = strtoul(params[1], &c, 0);
    }

    boot(r1, 0, 0, addr);
}

/**
 * @brief Shell handler for 'serialboot' command.
 * Usage: serialboot
 */
void serialboot_handler(int nb_params, char **params)
{
    (void)nb_params; (void)params;
    serialboot();
}