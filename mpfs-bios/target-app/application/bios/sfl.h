/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : sfl.h
 * Description: Serial Framing Protocol (SFL) constants and packet structures.
 *******************************************************************************/

// This file is Copyright (c) 2012-2015 Sebastien Bourdeauducq <sb@m-labs.hk>
// License: BSD

#ifndef __SFL_H
#define __SFL_H

/* Handshake Sync Magic Strings */
#define SFL_MAGIC_LEN 14
#define SFL_MAGIC_REQ "sL5DdSMmkekro\n"
#define SFL_MAGIC_ACK "z6IHG7cYDID6o\n"

/**
 * @struct sfl_frame
 * @brief Packed layout of Serial Framing Protocol network frame.
 */
struct sfl_frame {
	unsigned char payload_length; /**< Size of payload field (0-255 bytes) */
	unsigned char crc[2];          /**< 16-bit payload CRC */
	unsigned char cmd;             /**< SFL command opcode */
	unsigned char payload[255];    /**< Frame payload data */
} __attribute__((packed));

/* General SFL Commands */
#define SFL_CMD_ABORT      0x00               /* Abort transmission and return to prompt without booting[cite: 4] */
#define SFL_CMD_LOAD       0x01               /* Load data payload chunk into designated destination address[cite: 4] */
#define SFL_CMD_JUMP       0x02               /* Complete download process and execute target app at entry point[cite: 4] */
#define SFL_CMD_FLASH      0x03               /* Configure protocol engine to route payload directly to SPI Flash[cite: 4] */
#define SFL_CMD_DONE       0x04               /* Terminate SPI Flash mode transmission[cite: 4] */
#define SFL_CMD_LOAD_SRAM  0x05               /* Load payload directly to target SRAM[cite: 4] */

/* Protocol Acknowledgment Characters */
#define SFL_ACK_SUCCESS    'K'                /* Frame received and verified successfully[cite: 4] */
#define SFL_ACK_CRCERROR   'C'                /* Frame CRC checksum mismatch error[cite: 4] */
#define SFL_ACK_UNKNOWN    'U'                /* Command byte not recognized by target[cite: 4] */
#define SFL_ACK_ERROR      'E'                /* Frame receiving timeout or general transaction error[cite: 4] */

#endif /* __SFL_H */