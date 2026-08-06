#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <stdint.h>

#define MAX_PARAM       8

#define SYSTEM_CMDS     0
#define BOOT_CMDS       1
#define MEM_CMDS        2
#define NB_OF_GROUPS    3

typedef void (*cmd_handler)(int nb_params, char **params);

struct command_struct {
    void (*func)(int nb_params, char **params);
    const char *name;
    const char *help;
    int group;
};

void command_init(void);
struct command_struct *command_dispatcher(char *command, int nb_params, char **params);
void command_help_show(void);

#endif // __COMMAND_H__