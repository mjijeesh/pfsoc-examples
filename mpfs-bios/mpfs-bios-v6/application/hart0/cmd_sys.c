#include <stdio.h>
#include "command.h"

void help_handler(int nb_params, char **params)
{
    (void)nb_params; (void)params;
    command_help_show();
}

void ident_handler(int nb_params, char **params)
{
    (void)nb_params; (void)params;
    printf("Ident: LiteX BIOS on Microchip PolarFire SoC (E51 Core)\n");
}