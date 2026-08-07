/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : xmodem.c
 * Description: Implementation of XMODEM-CRC / XMODEM-1K protocol reception routines.
 *******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include "xmodem.h"
#include "uart.h"

/* XMODEM Control Characters */
#define SOH 0x01      /* Start of Header (128-byte block) */
#define STX 0x02      /* Start of TeXt (1024-byte block) */
#define EOT 0x04      /* End of Transmission */
#define ACK 0x06      /* Acknowledge */
#define NAK 0x15      /* Negative Acknowledge */
#define CAN 0x18      /* Cancel transfer */
#define C_CHAR 'C'    /* ASCII 'C' to initiate CRC16 mode */

#define MAX_RETR 25   /* Maximum retry attempts before timing out */

/**
 * @brief Computes 16-bit CRC CCITT (Polynomial 0x1021) over a data buffer.
 * @param buf Pointer to data buffer.
 * @param len Length of buffer in bytes.
 * @return Computed 16-bit CRC value.
 */
static uint16_t crc16_ccitt(const uint8_t *buf, int len)
{
    uint16_t crc = 0;
    while (len--) {
        crc ^= (uint16_t)*buf++ << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief Reads a single byte from UART with a software spin-loop timeout.
 * @param c Pointer to store received byte.
 * @param timeout_loops Loop iteration limit for polling RX ready.
 * @return 0 on successful read, -1 on timeout.
 */
static int uart_read_timeout(uint8_t *c, uint32_t timeout_loops)
{
    while (timeout_loops--) {
        if (uart_read_nonblock()) {
            *c = (uint8_t)uart_read();
            return 0; // Success
        }
    }
    return -1; // Timeout
}

/**
 * @brief Main XMODEM reception loop.
 * @param dest Memory buffer destination pointer.
 * @return Total received bytes on success, -1 on timeout or protocol error.
 */
int xmodem_receive(uint8_t *dest)
{
    uint8_t pnum = 1;
    uint8_t packet[1024];
    uint8_t header, blk_num, blk_inv, chk_hi, chk_lo;
    uint32_t total_bytes = 0;
    int retry;

    printf("\r\nWaiting for XMODEM transfer... (Press Ctrl+C or CAN to cancel)\r\n");

    /* Initiate CRC-mode transfer by repeatedly transmitting 'C' */
    for (retry = 0; retry < MAX_RETR; retry++) {
        uart_write(C_CHAR);
        if (uart_read_timeout(&header, 50000000) == 0) {
            break;
        }
    }

    if (retry == MAX_RETR) {
        printf("\r\nXMODEM: Sync Timeout!\r\n");
        return -1;
    }

    while (1) {
        /* End of Transmission check */
        if (header == EOT) {
            uart_write(ACK);
            printf("\r\nXMODEM Transfer Complete! Received %lu bytes.\r\n", (unsigned long)total_bytes);
            return (int)total_bytes;
        }

        /* Verify header type (128-byte or 1024-byte frame) */
        if (header != SOH && header != STX) {
            uart_write(NAK);
            if (uart_read_timeout(&header, 50000000) != 0) return -1;
            continue;
        }

        uint16_t pkt_len = (header == SOH) ? 128 : 1024;

        /* Fetch block sequence numbers */
        if (uart_read_timeout(&blk_num, 10000000) != 0) return -1;
        if (uart_read_timeout(&blk_inv, 10000000) != 0) return -1;

        /* Fetch payload bytes */
        for (int i = 0; i < pkt_len; i++) {
            if (uart_read_timeout(&packet[i], 10000000) != 0) return -1;
        }

        /* Fetch 16-bit CRC bytes */
        if (uart_read_timeout(&chk_hi, 10000000) != 0) return -1;
        if (uart_read_timeout(&chk_lo, 10000000) != 0) return -1;

        uint16_t expected_crc = ((uint16_t)chk_hi << 8) | chk_lo;
        uint16_t actual_crc = crc16_ccitt(packet, pkt_len);

        /* Validate packet index, bitwise complement integrity, and CRC match */
        if ((blk_num == pnum) && ((uint8_t)(blk_num + blk_inv) == 0xFF) && (expected_crc == actual_crc)) {
            for (int i = 0; i < pkt_len; i++) {
                dest[total_bytes++] = packet[i];
            }
            pnum++;
            uart_write(ACK);
        } else {
            /* Signal packet corrupt or out of sequence */
            uart_write(NAK);
        }

        /* Wait for next packet header */
        if (uart_read_timeout(&header, 50000000) != 0) return -1;
    }
}