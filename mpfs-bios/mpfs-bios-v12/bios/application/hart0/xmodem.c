#include <stdio.h>
#include <stdint.h>
#include "xmodem.h"
#include "uart.h"

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define C_CHAR 'C'

#define MAX_RETR 25

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

// Blocking read with simple loop timeout
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

int xmodem_receive(uint8_t *dest)
{
    uint8_t pnum = 1;
    uint8_t packet[1024];
    uint8_t header, blk_num, blk_inv, chk_hi, chk_lo;
    uint32_t total_bytes = 0;
    int retry;

    printf("\r\nWaiting for XMODEM transfer... (Press Ctrl+C or CAN to cancel)\r\n");

    // Initiate CRC-mode transfer by sending 'C'
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
        if (header == EOT) {
            uart_write(ACK);
            printf("\r\nXMODEM Transfer Complete! Received %lu bytes.\r\n", (unsigned long)total_bytes);
            return (int)total_bytes;
        }

        if (header != SOH && header != STX) {
            // Unexpected header, send NAK and request next
            uart_write(NAK);
            if (uart_read_timeout(&header, 50000000) != 0) return -1;
            continue;
        }

        uint16_t pkt_len = (header == SOH) ? 128 : 1024;

        // Read Block Number & Inverted Block Number
        if (uart_read_timeout(&blk_num, 10000000) != 0) return -1;
        if (uart_read_timeout(&blk_inv, 10000000) != 0) return -1;

        // Read Payload
        for (int i = 0; i < pkt_len; i++) {
            if (uart_read_timeout(&packet[i], 10000000) != 0) return -1;
        }

        // Read CRC16
        if (uart_read_timeout(&chk_hi, 10000000) != 0) return -1;
        if (uart_read_timeout(&chk_lo, 10000000) != 0) return -1;

        uint16_t expected_crc = ((uint16_t)chk_hi << 8) | chk_lo;
        uint16_t actual_crc = crc16_ccitt(packet, pkt_len);

        // Validate packet
        if ((blk_num == pnum) && ((uint8_t)(blk_num + blk_inv) == 0xFF) && (expected_crc == actual_crc)) {
            // Copy payload to destination memory
            for (int i = 0; i < pkt_len; i++) {
                dest[total_bytes++] = packet[i];
            }
            pnum++;
            uart_write(ACK);
        } else {
            // Corrupt or duplicate packet, request retry
            uart_write(NAK);
        }

        // Fetch header of next frame
        if (uart_read_timeout(&header, 50000000) != 0) return -1;
    }
}