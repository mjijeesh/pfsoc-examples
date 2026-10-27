/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : uart.h
 * Description: Low-level serial abstraction interface for Microchip MMUART.
 *******************************************************************************/

#ifndef __UART_H
#define __UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the MMUART hardware peripheral and configures baud rate.
 */
void uart_init(void);

/**
 * @brief Blocks execution until all pending bytes in the TX FIFO are transmitted.
 */
void uart_sync(void);

/**
 * @brief Transmits a single character over UART (polled mode).
 * @param c Character byte to transmit.
 */
void uart_write(char c);

/**
 * @brief Reads a single character from UART (blocking mode).
 * @return Received character byte.
 */
char uart_read(void);

/**
 * @brief Checks if a character is available in the UART RX buffer.
 * @return 1 if data is available, 0 if buffer is empty.
 */
int uart_read_nonblock(void);

#ifdef __cplusplus
}
#endif

#endif // __UART_H