/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : xmodem.h
 * Description: Interface definitions for XMODEM file transfer protocol receiver.
 *******************************************************************************/

#ifndef __XMODEM_H
#define __XMODEM_H

#include <stdint.h>

/**
 * @brief Receives a payload over UART using the XMODEM-CRC / XMODEM-1K protocol.
 * @param dest Pointer to the destination memory address where the binary image will be loaded.
 * @return Total number of bytes received on success, or -1 on failure/timeout.
 */
int xmodem_receive(uint8_t *dest);

#endif // __XMODEM_H