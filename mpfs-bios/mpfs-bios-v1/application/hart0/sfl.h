#ifndef __SFL_H
#define __SFL_H

#include <stdint.h>

#define SFL_MAGIC_REQ "sfl"
#define SFL_MAGIC_ACK "sflack"
#define SFL_MAGIC_LEN 6

#define SFL_CMD_ABORT 0x00
#define SFL_CMD_LOAD  0x01
#define SFL_CMD_JUMP  0x02

#define SFL_ACK_SUCCESS  'K'
#define SFL_ACK_CRCERROR 'C'
#define SFL_ACK_ERROR    'E'
#define SFL_ACK_UNKNOWN  'U'

struct sfl_frame {
    uint8_t payload_length;
    uint8_t crc[2];
    uint8_t cmd;
    uint8_t payload[255];
} __attribute__((packed));

#endif // __SFL_H