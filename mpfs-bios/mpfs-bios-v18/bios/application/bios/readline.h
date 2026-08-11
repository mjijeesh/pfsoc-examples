/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : readline.h
 * Description: Interactive Command Line Interface line editing definitions.
 *******************************************************************************/

#ifndef __READLINE_H
#define __READLINE_H

#include <stdint.h>

#define CMD_LINE_BUFFER_SIZE 128
#define HIST_MAX             16
#define PROMPT               "pfsoc-bios> "

/* VT100 / Control Key Command Encodings */
#define KEY_UP               0x101
#define KEY_DOWN             0x102
#define KEY_RIGHT            0x103
#define KEY_LEFT             0x104
#define KEY_HOME             0x105
#define KEY_END              0x106
#define KEY_INSERT           0x107
#define KEY_DEL              0x108
#define KEY_PAGEUP           0x109
#define KEY_PAGEDOWN         0x10A
#define KEY_ERASE_TO_EOL     0x10B
#define KEY_REFRESH_TO_EOL   0x10C
#define KEY_ERASE_LINE       0x10D
#define KEY_CLEAR_SCREEN     0x10E

#define CTL_BACKSPACE        '\b'
#define DEL                  127
#define KEY_DEL7             8

#define CTL_CH(c)            ((c) - 'a' + 1)
#define ANSI_CLEAR_SCREEN    "\033[2J\033[H"

/**
 * @struct esc_cmds
 * @brief Maps ANSI terminal escape sequence strings to control keys.
 */
struct esc_cmds {
    const char *seq;
    int val;
};

/**
 * @brief Resets command line history ring buffer indices.
 */
void hist_init(void);

/**
 * @brief Reads an interactive line of input from serial console with full editing features.
 * @param buf Target string buffer.
 * @param len Maximum buffer length.
 * @return Length of line entered, or -1 on cancel.
 */
int readline(char *buf, int len);

/**
 * @brief Registers an idle hook callback executed while waiting for user keypresses.
 * @param fptr Callback function pointer.
 */
void set_idle_hook(void (*fptr)(void));

#endif // __READLINE_H