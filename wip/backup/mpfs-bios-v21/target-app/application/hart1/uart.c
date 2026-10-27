#include <stdio.h>
#include "uart.h"
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"
#include "inc/uart_mapping.h"

// Microchip HAL UART instance assigned to Hart 1 (U54_1)
extern struct mss_uart_instance* p_uartmap_u54_1;

// Single-character lookahead buffer for non-blocking reads
static int has_lookahead = 0;
static char lookahead_char = 0;

void uart_init(void)
{
    has_lookahead = 0;

    // 1. Enable Clock and Reset for U54_1 MMUART1
    (void) mss_config_clk_rst(MSS_PERIPH_MMUART_U54_1, (uint8_t) 1, PERIPHERAL_ON);

    // 2. Initialize MMUART1 (115200 8N1) using official Microchip driver
    MSS_UART_init(p_uartmap_u54_1,
                  MSS_UART_115200_BAUD,
                  MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
}

void uart_write(char c)
{
    uint8_t ch = (uint8_t)c;
    // Transmit character using polled mode
    MSS_UART_polled_tx(p_uartmap_u54_1, &ch, 1);
}

char uart_read(void)
{
    // If a character was already fetched during a nonblock check, return it
    if (has_lookahead) {
        has_lookahead = 0;
        return lookahead_char;
    }

    uint8_t rx_byte;
    size_t rx_size = 0;

    // Wait until 1 character is received
    while (rx_size == 0) {
        rx_size = MSS_UART_get_rx(p_uartmap_u54_1, &rx_byte, 1);
    }

    return (char)rx_byte;
}

int uart_read_nonblock(void)
{
    if (has_lookahead) {
        return 1;
    }

    uint8_t rx_byte;
    size_t rx_size = MSS_UART_get_rx(p_uartmap_u54_1, &rx_byte, 1);

    if (rx_size > 0) {
        lookahead_char = (char)rx_byte;
        has_lookahead = 1;
        return 1;
    }

    return 0;
}

void uart_sync(void)
{
    // Wait for transmit complete
    while (0u == MSS_UART_tx_complete(p_uartmap_u54_1)) {
        ;
    }
}

/*
 * Newlib standard library syscall overrides:
 * Redirects C standard library printf() output to LiteX uart_write()
 */
int _write(int fd, const char *buf, int count)
{
    (void)fd;
    for (int i = 0; i < count; i++) {
        uart_write(buf[i]);
    }
    return count;
}

int _read1(int fd, char *buf, int count)
{
    (void)fd;
    for (int i = 0; i < count; i++) {
        buf[i] = uart_read();
    }
    return count;
}