#ifndef __READLINE_H
#define __READLINE_H

#include <stdint.h>

#define CMD_LINE_BUFFER_SIZE 128
#define HIST_MAX             16
#define PROMPT               "litex> "

// VT100 / Control Key Codes
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

struct esc_cmds {
    const char *seq;
    int val;
};

void hist_init(void);
int readline(char *buf, int len);
void set_idle_hook(void (*fptr)(void));

#endif // __READLINE_H