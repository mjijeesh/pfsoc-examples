/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : hw_info.h
 * Description: Header file for system hardware information command display.
 *******************************************************************************/

#ifndef __HW_INFO_H__
#define __HW_INFO_H__

/**
 * @brief Displays system hardware description, clocks, memory map, and active peripherals.
 */
void show_hw_info(void);

/**
 * @brief Shell command handler entry point for 'hw_info'.
 * @param nb_params Parameter count.
 * @param params Array of parameter string pointers.
 */
void hw_info_handler(int nb_params, char **params);

#endif // __HW_INFO_H__