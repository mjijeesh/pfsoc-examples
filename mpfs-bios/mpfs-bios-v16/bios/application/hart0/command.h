/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : command.h
 * Description: Command table definitions and command dispatcher interfaces.
 *******************************************************************************/

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <stdint.h>

#define MAX_PARAM       8    /* Maximum shell command parameters */

/* Command Group Identifiers */
#define SYSTEM_CMDS     0    /* System management commands */
#define BOOT_CMDS       1    /* Image boot commands */
#define MEM_CMDS        2    /* Memory operation commands */
#define NB_OF_GROUPS    3    /* Total command group count */

typedef void (*cmd_handler)(int nb_params, char **params);

/**
 * @struct command_struct
 * @brief BIOS shell command registration record[cite: 28].
 */
struct command_struct {
    void (*func)(int nb_params, char **params); /**< Pointer to command handler function[cite: 28] */
    const char *name;                            /**< Command invocation string[cite: 28] */
    const char *help;                            /**< Help description string[cite: 28] */
    int group;                                   /**< Command category group ID[cite: 28] */
};

/**
 * @brief Initializes shell command dispatch sub-system[cite: 28].
 */
void command_init(void);

/**
 * @brief Matches and dispatches parsed user command string[cite: 28].
 * @param command Command keyword string[cite: 28].
 * @param nb_params Parameter count[cite: 28].
 * @param params Array of parameter string pointers[cite: 28].
 * @return Pointer to command struct if found, NULL if command is invalid[cite: 28].
 */
struct command_struct *command_dispatcher(char *command, int nb_params, char **params);

/**
 * @brief Prints categorized help menu of all available BIOS commands[cite: 28].
 */
void command_help_show(void);

#endif // __COMMAND_H__