/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * File Name  : uart.c
 * Description: Low-level serial abstraction interface driven by board_config.h
 *******************************************************************************/

#include <stdio.h>
#include "uart.h"
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"

/* Single-character lookahead buffer for non-blocking read checks */
static int has_lookahead = 0;
static char lookahead_char = 0;

/**
 * @brief Initializes the MMUART hardware peripheral configured in soc.h
 */
void uart_init(void)
{
    has_lookahead = 0;

    /* 1. Enable Clock & Reset for the board-configured MMUART instance */
    (void) mss_config_clk_rst(CONSOLE_UART_PERIPH, (uint8_t)1, PERIPHERAL_ON);

    /* 2. Configure board UART instance with baud rate and 8N1 frame format */
    MSS_UART_init(CONSOLE_UART_INSTANCE,
                  CONSOLE_UART_BAUD,
                  MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
}

/**
 * @brief Transmits a byte over the active board UART instance.
 */
void uart_write(char c)
{
    uint8_t ch = (uint8_t)c;
    MSS_UART_polled_tx(CONSOLE_UART_INSTANCE, &ch, 1);
}

/**
 * @brief Reads a byte from the active board UART instance (blocking).
 */
char uart_read(void)
{
    if (has_lookahead) {
        has_lookahead = 0;
        return lookahead_char;
    }

    uint8_t rx_byte = 0;
    size_t rx_size = 0;

    while (rx_size == 0) {
        rx_size = MSS_UART_get_rx(CONSOLE_UART_INSTANCE, &rx_byte, 1);
    }

    return (char)rx_byte;
}

/**
 * @brief Polls the active board UART instance for incoming data (non-blocking).
 */
int uart_read_nonblock(void)
{
    if (has_lookahead) {
        return 1;
    }

    uint8_t rx_byte = 0;
    size_t rx_size = MSS_UART_get_rx(CONSOLE_UART_INSTANCE, &rx_byte, 1);

    if (rx_size > 0) {
        lookahead_char = (char)rx_byte;
        has_lookahead = 1;
        return 1;
    }

    return 0;
}

/**
 * @brief Synchronizes and flushes the TX FIFO buffer on the active board UART.
 */
void uart_sync(void)
{
    while (0u == MSS_UART_tx_complete(CONSOLE_UART_INSTANCE)) {
        ;
    }
}

/**
 * @brief Newlib C standard library system call override for printf redirect.
 */
int _write(int fd, const char *buf, int count)
{
    (void)fd;
    for (int i = 0; i < count; i++) {
        uart_write(buf[i]);
    }
    return count;
}

/**
 * @brief Newlib C standard library system call override for stdin redirect.
 */
int _read1(int fd, char *buf, int count)
{
    (void)fd;
    for (int i = 0; i < count; i++) {
        buf[i] = uart_read();
    }
    return count;
}