#ifndef UART_H
#define UART_H

/* Hardware Driver Prototypes */
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
char uart_getc(void);
int check_command(const char *expected);

#endif /* UART_H */