#include <stdio.h>
#include "uart.h"
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"
#include "inc/uart_mapping.h"


/* 
 * p_uartmap_e51 and MSS_PERIPH_MMUART_E51 are dynamically assigned in 
 * 'inc/uart_mapping.h' based on the preprocessor macro injected by the Makefile:
 *   - MPFS_DISCOVERY_KIT -> MMUART1 (&g_mss_uart1_lo)
 *   - MPFS_ICICLE_KIT_ES -> MMUART0 (&g_mss_uart0_lo)
 *   - MPFS_VIDEO_KIT     -> MMUART0 (&g_mss_uart0_lo)
 */

// Microchip HAL UART instance assigned to Hart 0 (E51)
extern struct mss_uart_instance* p_uartmap_e51;

/* Single-character lookahead buffer for non-blocking read checks */
static int has_lookahead = 0;
static char lookahead_char = 0;


/**
 * @brief Dynamically initializes the E51 MMUART assigned to the target board.
 */
void uart_init(void)
{
    has_lookahead = 0;

    /* 1. Enable Clock & Reset for the board-specific MMUART instance */
    (void) mss_config_clk_rst(MSS_PERIPH_MMUART_E51, (uint8_t) 1, PERIPHERAL_ON);

    /* 2. Configure board UART instance for 115200 baud, 8N1 */
    MSS_UART_init(p_uartmap_e51,
                  MSS_UART_115200_BAUD,
                  MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
}

/**
 * @brief Transmits a byte over the active board UART instance.
 */
void uart_write(char c)
{
    uint8_t ch = (uint8_t)c;
    MSS_UART_polled_tx(p_uartmap_e51, &ch, 1);
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

    uint8_t rx_byte;
    size_t rx_size = 0;

    while (rx_size == 0) {
        rx_size = MSS_UART_get_rx(p_uartmap_e51, &rx_byte, 1);
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

    uint8_t rx_byte;
    size_t rx_size = MSS_UART_get_rx(p_uartmap_e51, &rx_byte, 1);

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
    while (0u == MSS_UART_tx_complete(p_uartmap_e51)) {
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