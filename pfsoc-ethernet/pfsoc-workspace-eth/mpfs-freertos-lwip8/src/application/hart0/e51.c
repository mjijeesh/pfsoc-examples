/*******************************************************************************
 * Copyright 2019 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * @file u54_1.c
 * @author Microchip FPGA Embedded Systems Solutions
 * @brief Application code running on u54_1
 *
 */

#include <stdio.h>
#include <string.h>
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"
#include "inc/common.h"
#include "inc/uart_mapping.h"


void e51(void) {
    /* Clear local trap registers */
    write_csr(mscratch, 0);
    write_csr(mcause, 0);
    write_csr(mepc, 0);

    /* Initialize PLIC for System */
    PLIC_init();

    /* Release U54_1 from boot hold and park E51 in low-power wait loop */
    raise_soft_interrupt(1); 
    while (1) {
        __asm__ volatile("wfi");
    }
}