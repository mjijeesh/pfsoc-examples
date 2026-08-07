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

/**
 * @brief Launches application across U54 harts via Inter-Processor Interrupts (IPI)[cite: 36, 37].
 * @param r1 Register parameter 1 passed to target image[cite: 36, 37].
 * @param r2 Register parameter 2 passed to target image[cite: 36, 37].
 * @param r3 Register parameter 3 passed to target image[cite: 36, 37].
 * @param addr Destination entry address for target application[cite: 36, 37].
 */
void boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr);

int boot_load_max_size(unsigned long addr, size_t *max_size);

/**
 * @brief Starts Serial Framing Protocol (SFL) bootloader state machine[cite: 36, 37].
 * @return 0 on success, non-zero on error or cancel.
 */
int serialboot(void);

#endif // __BOOT_H