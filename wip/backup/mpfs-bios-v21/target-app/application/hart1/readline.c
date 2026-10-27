#include <stdio.h>
#include "readline.h"
#include "uart.h"

int readline(char *s, int maxlen)
{
    char c;
    char *p = s;

    while (1) {
        c = uart_read();

        // Newline or Carriage Return terminates line entry
        if ((c == '\r') || (c == '\n')) {
            *p = '\0';
            uart_write('\r');
            uart_write('\n');
            return p - s;
        } 
        // Backspace / Delete
        else if ((c == '\b') || (c == 0x7F)) {
            if (p > s) {
                p--;
                uart_write('\b');
                uart_write(' ');
                uart_write('\b');
            }
        } 
        // Printable ASCII
        else if ((c >= 32) && (c <= 126)) {
            if (p < (s + maxlen - 1)) {
                *p++ = c;
                uart_write(c);
            }
        }
    }
}