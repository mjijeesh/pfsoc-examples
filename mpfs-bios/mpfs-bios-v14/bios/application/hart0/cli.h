/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : cli.h
 * Description: Command Line Interface (CLI) initialization and execution interface.
 *******************************************************************************/

#ifndef __CLI_H
#define __CLI_H

/**
 * @brief Displays CLI boot banner and initializes console interface.
 */
void cli_init(void);

/**
 * @brief Executes interactive Command Line Interface processing loop.
 */
void cli_run(void);

#endif // __CLI_H