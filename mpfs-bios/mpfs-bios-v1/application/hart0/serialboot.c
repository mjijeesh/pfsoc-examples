#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "sfl.h"
#include "serialboot.h"
#include "uart.h"

#define MAX_FAILURES 256

// Helper to get RISC-V time cycles (used for timeout tracking)
static inline uint64_t get_time_ticks(void) {
    uint64_t ticks;
    __asm__ volatile ("rdtime %0" : "=r"(ticks));
    return ticks;
}

// CRC16 CCITT used by LiteX SFL
static uint16_t crc16(const unsigned char *buffer, int len) {
    uint16_t crc = 0;
    while (len--) {
        crc ^= (uint16_t)*buffer++ << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

static uint32_t get_uint32(const unsigned char *data) {
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8)  |
            (uint32_t)data[3];
}

// Boot jumper for PolarFire SoC E51 Core
void boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr) {
    printf("\r\nExecuting booted program at 0x%08lx...\r\n\r\n", addr);
    uart_sync();

    // Flush RISC-V Pipeline & Instruction Cache
    __asm__ volatile ("fence.i" ::: "memory");
    __asm__ volatile ("fence" ::: "memory");

    typedef void (*entry_fn)(unsigned long, unsigned long, unsigned long);
    entry_fn entry = (entry_fn)addr;
    entry(r1, r2, r3);

    while (1);
}

enum {
    ACK_TIMEOUT,
    ACK_CANCELLED,
    ACK_OK
};

static int check_ack(uint64_t timeout_ticks) {
    int recognized = 0;
    static const char str[SFL_MAGIC_LEN] = SFL_MAGIC_ACK;
    uint64_t start = get_time_ticks();

    while ((get_time_ticks() - start) < timeout_ticks) {
        if (uart_read_nonblock()) {
            char c = uart_read();
            if ((c == 'Q') || (c == 0x1B)) // Escape / 'Q' cancels
                return ACK_CANCELLED;
            if (c == str[recognized]) {
                recognized++;
                if (recognized == SFL_MAGIC_LEN)
                    return ACK_OK;
            } else {
                recognized = (c == str[0]) ? 1 : 0;
            }
        }
    }
    return ACK_TIMEOUT;
}

static int serialboot_fail(int *failures) {
    (*failures)++;
    if (*failures >= MAX_FAILURES) {
        printf("Too many consecutive errors, aborting\r\n");
        return 1;
    }
    return 0;
}

int serialboot(void) {
    struct sfl_frame frame;
    int failures = 0;
    static const char str[SFL_MAGIC_LEN + 1] = SFL_MAGIC_REQ;
    const char *c;
    int ack_status;

    printf("\r\nBooting from serial (SFL protocol)...\r\n");
    printf("Press Q or ESC to abort boot.\r\n");

    // Send the serialboot "magic" request to host and wait for ACK
    c = str;
    while (*c) {
        uart_write(*c);
        c++;
    }

    // 1 Million ticks ~1 second timeout (depending on mtime rate)
    ack_status = check_ack(1000000);
    if (ack_status == ACK_TIMEOUT) {
        printf("Timeout waiting for host ACK.\r\n");
        return 1;
    }
    if (ack_status == ACK_CANCELLED) {
        printf("Cancelled by user.\r\n");
        return 0;
    }

    /* ACK_OK received */
    while (1) {
        int i = 0;
        int timeout = 1;
        uint64_t frame_start = get_time_ticks();
        uint64_t frame_timeout = 2000000; // 2 sec inter-frame timeout

        while ((get_time_ticks() - frame_start) < frame_timeout) {
            if (uart_read_nonblock()) {
                unsigned char data = uart_read();
                frame_start = get_time_ticks(); // Reset inter-byte timeout

                if (i == 0) frame.payload_length = data;
                if (i == 1) frame.crc[0] = data;
                if (i == 2) frame.crc[1] = data;
                if (i == 3) frame.cmd = data;
                if (i >= 4) {
                    frame.payload[i - 4] = data;
                }
                if (i == (frame.payload_length + 4 - 1)) {
                    timeout = 0;
                    break;
                }
                i++;
            }
        }

        if (timeout) {
            uart_write(SFL_ACK_ERROR);
            if (serialboot_fail(&failures)) return 1;
            continue;
        }

        int received_crc = ((int)frame.crc[0] << 8) | (int)frame.crc[1];
        int computed_crc = crc16(&frame.cmd, frame.payload_length + 1);
        if (computed_crc != received_crc) {
            uart_write(SFL_ACK_CRCERROR);
            if (serialboot_fail(&failures)) return 1;
            continue;
        }

        switch (frame.cmd) {
            case SFL_CMD_ABORT:
                failures = 0;
                uart_write(SFL_ACK_SUCCESS);
                return 1;

            case SFL_CMD_LOAD: {
                if (frame.payload_length < 4) {
                    uart_write(SFL_ACK_ERROR);
                    if (serialboot_fail(&failures)) return 1;
                    break;
                }
                failures = 0;

                char *load_addr = (char *)(uintptr_t)get_uint32(&frame.payload[0]);
                uint32_t load_size = frame.payload_length - 4;

                memcpy(load_addr, &frame.payload[4], load_size);
                uart_write(SFL_ACK_SUCCESS);
                break;
            }

            case SFL_CMD_JUMP: {
                if (frame.payload_length < 4) {
                    uart_write(SFL_ACK_ERROR);
                    if (serialboot_fail(&failures)) return 1;
                    break;
                }
                failures = 0;
                uart_write(SFL_ACK_SUCCESS);
                uint32_t jump_addr = get_uint32(&frame.payload[0]);
                boot(0, 0, 0, jump_addr);
                break;
            }

            default:
                uart_write(SFL_ACK_UNKNOWN);
                if (serialboot_fail(&failures)) return 1;
                break;
        }
    }
    return 1;
}