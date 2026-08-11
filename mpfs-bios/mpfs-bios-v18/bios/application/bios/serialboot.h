/*******************************************************************************
 * Company    : Tecnomic Components
 * Author     : Jijeesh M
 * E-mail     : jijeesh@tecnomic.com
 * File Name  : serialboot.h
 * Description: Serial Bootloader function interfaces.
 *******************************************************************************/

#ifndef __SERIALBOOT_H
#define __SERIALBOOT_H

/**
 * @brief Initiates serial boot reception over MMUART using SFL protocol.
 * @return 0 on success, non-zero on error or abort.
 */
int serialboot(void);

/**
 * @brief Boots target application across RISC-V hart coreplex.
 * @param r1 Register parameter 1 passed to target image.
 * @param r2 Register parameter 2 passed to target image.
 * @param r3 Register parameter 3 passed to target image.
 * @param addr Memory jump entry address.
 */
void boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr);

#endif // __SERIALBOOT_H