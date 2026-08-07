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

/* Protocol Command Opcodes */
#define SFL_CMD_ABORT		0x00
#define SFL_CMD_LOAD		0x01
#define SFL_CMD_JUMP		0x02

/* Protocol Response Acknowledgment Characters */
#define SFL_ACK_SUCCESS		'K'
#define SFL_ACK_CRCERROR	'C'
#define SFL_ACK_UNKNOWN		'U'
#define SFL_ACK_ERROR		'E'

#endif /* __SFL_H */