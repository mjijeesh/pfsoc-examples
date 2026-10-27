/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : boot.h
 * Description: Header definitions for system boot utilities and serial loader.
 *******************************************************************************/

#ifndef __BOOT_H
#define __BOOT_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Launches application across U54 harts via Inter-Processor Interrupts (IPI)[cite: 13].
 * @param r1 Register parameter 1 passed to target image[cite: 13].
 * @param r2 Register parameter 2 passed to target image[cite: 13].
 * @param r3 Register parameter 3 passed to target image[cite: 13].
 * @param addr Destination entry address for target application[cite: 13].
 */
void boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr);

int boot_load_max_size(unsigned long addr, size_t *max_size);

/**
 * @brief Starts Serial Framing Protocol (SFL) bootloader state machine[cite: 13].
 * Handles both direct RAM download and direct SPI Flash flashing.
 * @return 0 on success, non-zero on error or cancel.
 */
int serialboot(void);

/**
 * @brief SFL entry point configured for direct SPI Flash target.
 * @return 0 on success, non-zero on error or cancel.
 */
int flashwrite(void);

void autoboot_run(int timeout_sec);

#endif // __BOOT_H