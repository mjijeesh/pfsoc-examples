#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "command.h"
#include "serialboot.h"

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

void serialboot_handler(int nb_params, char **params)
{
    (void)nb_params; (void)params;
    serialboot();
}