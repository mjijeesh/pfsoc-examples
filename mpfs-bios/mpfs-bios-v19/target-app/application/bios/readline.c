/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : readline.c
 * Description: Terminal line editor implementation with VT100 key bindings & history.
 *******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "readline.h"
#include "uart.h"

/* Command history ring buffer storage */
static int hist_max = 0;
static int hist_add_idx = 0;
static int hist_cur = 0;
static int hist_num = 0;
static char hist_lines[HIST_MAX][CMD_LINE_BUFFER_SIZE];

static void (*idle_hook_ptr)(void) = NULL;

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

/* ANSI sequence lookup table */
static const struct esc_cmds esccmds[] = {
    {"OA",   KEY_UP},
    {"OB",   KEY_DOWN},
    {"OC",   KEY_RIGHT},
    {"OD",   KEY_LEFT},
    {"OH",   KEY_HOME},
    {"OF",   KEY_END},
    {"[A",   KEY_UP},
    {"[B",   KEY_DOWN},
    {"[C",   KEY_RIGHT},
    {"[D",   KEY_LEFT},
    {"[H",   KEY_HOME},
    {"[F",   KEY_END},
    {"[1~",  KEY_HOME},
    {"[2~",  KEY_INSERT},
    {"[3~",  KEY_DEL},
    {"[4~",  KEY_END},
    {"[5~",  KEY_PAGEUP},
    {"[6~",  KEY_PAGEDOWN},
};

static inline void getcmd_putch(char c) {
    uart_write(c);
}

static inline void getcmd_cbeep(void) {
    uart_write('\a');
}

static void putnstr(const char *str, unsigned int n) {
    while (n--) {
        uart_write(*str++);
    }
}

/* Line Editing Utility Macros */
#define BEGINNING_OF_LINE() do { \
    while (num) { \
        getcmd_putch(CTL_BACKSPACE); \
        num--; \
    } \
} while (0)

#define ERASE_TO_EOL() do { \
    wlen = eol_num - num; \
    if (wlen) { \
        memset(buf + num, ' ', wlen); \
        putnstr(buf + num, wlen); \
        while (wlen--) getcmd_putch(CTL_BACKSPACE); \
        eol_num = num; \
        buf[eol_num] = 0; \
    } \
} while (0)

#define REFRESH_TO_EOL() do { \
    if (num < eol_num) { \
        putnstr(buf + num, eol_num - num); \
        num = eol_num; \
    } \
} while (0)

void set_idle_hook(void (*fptr)(void)) {
    idle_hook_ptr = fptr;
}

/**
 * @brief Processes next key input, parsing ANSI escape sequences into control codes.
 * @return Key integer or mapped control key constant.
 */
static int read_key(void) {
    char c;
    char esc[8];

    if (idle_hook_ptr != NULL) {
        while (!uart_read_nonblock()) {
            idle_hook_ptr();
        }
    }

    c = uart_read();

    if (c == 27) { // ESC sequence detection
        unsigned int i = 0;
        esc[i++] = uart_read();
        esc[i++] = uart_read();
        if (isdigit((unsigned char)esc[1])) {
            while (1) {
                if (i == ARRAY_SIZE(esc) - 1)
                    return -1;
                esc[i] = uart_read();
                if ((esc[i] >= 0x40) && (esc[i] <= 0x7E)) {
                    i++;
                    break;
                }
                i++;
            }
        }
        esc[i] = 0;
        for (i = 0; i < ARRAY_SIZE(esccmds); i++) {
            if (!strcmp(esc, esccmds[i].seq))
                return esccmds[i].val;
        }
        return -1;
    }
    return c;
}

static void cread_add_to_hist(char *line) {
    strncpy(&hist_lines[hist_add_idx][0], line, CMD_LINE_BUFFER_SIZE - 1);
    hist_lines[hist_add_idx][CMD_LINE_BUFFER_SIZE - 1] = 0;

    if (++hist_add_idx >= HIST_MAX)
        hist_add_idx = 0;

    if (hist_add_idx > hist_max)
        hist_max = hist_add_idx;

    hist_num++;
}

static char* hist_prev(void) {
    char *ret;
    int old_cur;

    if (hist_cur < 0)
        return NULL;

    old_cur = hist_cur;
    if (--hist_cur < 0)
        hist_cur = hist_max;

    if (hist_cur == hist_add_idx) {
        hist_cur = old_cur;
        ret = NULL;
    } else {
        ret = &hist_lines[hist_cur][0];
    }

    return ret;
}

static char* hist_next(void) {
    char *ret;

    if (hist_cur < 0)
        return NULL;

    if (hist_cur == hist_add_idx)
        return NULL;

    if (++hist_cur > hist_max)
        hist_cur = 0;

    if (hist_cur == hist_add_idx)
        ret = "";
    else
        ret = &hist_lines[hist_cur][0];

    return ret;
}

void hist_init(void) {
    hist_max = 0;
    hist_add_idx = 0;
    hist_cur = -1;
    hist_num = 0;

    for (int i = 0; i < HIST_MAX; i++)
        hist_lines[i][0] = '\0';
}

static void cread_add_char(char ichar, int insert, unsigned int *num,
                           unsigned int *eol_num, char *buf, unsigned int len) {
    unsigned int wlen;

    if (insert || *num == *eol_num) {
        if (*eol_num >= len - 1) {
            getcmd_cbeep();
            return;
        }
        (*eol_num)++;
    }

    if (insert) {
        wlen = *eol_num - *num;
        if (wlen > 1) {
            memmove(&buf[*num + 1], &buf[*num], wlen - 1);
        }

        buf[*num] = ichar;
        putnstr(buf + *num, wlen);
        (*num)++;
        while (--wlen) {
            getcmd_putch(CTL_BACKSPACE);
        }
    } else {
        wlen = 1;
        buf[*num] = ichar;
        putnstr(buf + *num, wlen);
        (*num)++;
    }
}

int readline(char *buf, int len) {
    unsigned int num = 0;
    unsigned int eol_num = 0;
    unsigned int wlen;
    int insert = 1;
    int ichar;

    if (len <= 0)
        return -1;

    while (1) {
        ichar = read_key();

        if (ichar < 0)
            continue;

        if ((ichar == '\n') || (ichar == '\r'))
            break;

        switch (ichar) {
        case KEY_HOME:
            BEGINNING_OF_LINE();
            break;

        case CTL_CH('c'):
            *buf = 0;
            return -1;

        case KEY_RIGHT:
            if (num < eol_num) {
                getcmd_putch(buf[num]);
                num++;
            }
            break;

        case KEY_LEFT:
            if (num) {
                getcmd_putch(CTL_BACKSPACE);
                num--;
            }
            break;

        case CTL_CH('d'):
            if (num < eol_num) {
                wlen = eol_num - num - 1;
                if (wlen) {
                    memmove(&buf[num], &buf[num + 1], wlen);
                    putnstr(buf + num, wlen);
                }
                getcmd_putch(' ');
                do {
                    getcmd_putch(CTL_BACKSPACE);
                } while (wlen--);
                eol_num--;
            }
            break;

        case KEY_ERASE_TO_EOL:
            ERASE_TO_EOL();
            break;

        case KEY_REFRESH_TO_EOL:
        case KEY_END:
            REFRESH_TO_EOL();
            break;

        case KEY_INSERT:
            insert = !insert;
            break;

        case KEY_ERASE_LINE:
            BEGINNING_OF_LINE();
            ERASE_TO_EOL();
            break;

        case KEY_CLEAR_SCREEN:
            printf(ANSI_CLEAR_SCREEN "%s", PROMPT);
            putnstr(buf, eol_num);
            wlen = eol_num - num;
            while (wlen--)
                getcmd_putch(CTL_BACKSPACE);
            break;

        case DEL:
        case KEY_DEL7:
            if (num) {
                wlen = eol_num - num;
                num--;
                memmove(buf + num, buf + num + 1, wlen);
                getcmd_putch(CTL_BACKSPACE);
                putnstr(buf + num, wlen);
                getcmd_putch(' ');
                do {
                    getcmd_putch(CTL_BACKSPACE);
                } while (wlen--);
                eol_num--;
            }
            break;

        case KEY_DEL:
            if (num < eol_num) {
                wlen = eol_num - num;
                memmove(buf + num, buf + num + 1, wlen);
                putnstr(buf + num, wlen - 1);
                getcmd_putch(' ');
                do {
                    getcmd_putch(CTL_BACKSPACE);
                } while (--wlen);
                eol_num--;
            }
            break;

        case KEY_UP:
        case KEY_DOWN: {
            char *hline;
            if (ichar == KEY_UP)
                hline = hist_prev();
            else
                hline = hist_next();

            if (!hline) {
                getcmd_cbeep();
                break;
            }

            BEGINNING_OF_LINE();
            ERASE_TO_EOL();

            strncpy(buf, hline, len - 1);
            buf[len - 1] = 0;
            eol_num = strlen(buf);
            REFRESH_TO_EOL();
            break;
        }

        default:
            if (isascii(ichar) && isprint(ichar))
                cread_add_char(ichar, insert, &num, &eol_num, buf, len);
            break;
        }
    }

    len = eol_num;
    buf[eol_num] = '\0';

    if (buf[0])
        cread_add_to_hist(buf);

    hist_cur = hist_add_idx;

    return len;
}