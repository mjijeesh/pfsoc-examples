/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : cmd_sys_services.c
 * Description: CLI Commands for PolarFire SoC System Services
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "command.h"
#include "mss_sys_services.h"

/**
 * @brief Command 'sys_serial'
 * Usage: sys_serial
 * Reads and prints the 128-bit (16-byte) Device Serial Number (DSN).
 */
void sys_serial_handler(int nb_params, char **params)
{
    (void)nb_params; (void)params;
    uint8_t serial[MSS_SYS_SERIAL_NUMBER_RESP_LEN] = {0};
    uint16_t status;

    MSS_SYS_select_service_mode(MSS_SYS_SERVICE_POLLING_MODE, NULL);
    status = MSS_SYS_get_serial_number(serial, 0);

    if (status == MSS_SYS_SUCCESS) {
        printf("Device Serial Number (DSN): ");
        for (int i = 0; i < MSS_SYS_SERIAL_NUMBER_RESP_LEN; i++) {
            printf("%02X", serial[i]);
        }
        printf("\n");
    } else {
        printf("Error: Failed to read Serial Number (Status Code: 0x%04X)\n", status);
    }
}

/**
 * @brief Command 'sys_info'
 * Usage: sys_info
 * Displays Usercode and FPGA Design Info (Design ID, Version, Back level).
 */
void sys_info_handler(int nb_params, char **params)
{
    (void)nb_params; (void)params;
    uint8_t usercode[MSS_SYS_USERCODE_RESP_LEN] = {0};
    uint8_t design_info[MSS_SYS_DESIGN_INFO_RESP_LEN] = {0};
    uint16_t status;

    MSS_SYS_select_service_mode(MSS_SYS_SERVICE_POLLING_MODE, NULL);

    /* 1. Read USERCODE */
    status = MSS_SYS_get_user_code(usercode, 0);
    if (status == MSS_SYS_SUCCESS) {
        uint32_t code = ((uint32_t)usercode[0])       |
                        ((uint32_t)usercode[1] << 8)  |
                        ((uint32_t)usercode[2] << 16) |
                        ((uint32_t)usercode[3] << 24);
        printf("USERCODE           : 0x%08X\n", (unsigned int)code);
    } else {
        printf("USERCODE           : Error (0x%04X)\n", status);
    }

    /* 2. Read Design Information */
    status = MSS_SYS_get_design_info(design_info, 0);
    if (status == MSS_SYS_SUCCESS) {
        printf("Design ID          : ");
        for (int i = 0; i < 32; i++) {
            printf("%02X", design_info[i]);
        }
        printf("\n");

        uint16_t design_ver = (design_info[33] << 8) | design_info[32];
        uint16_t back_level = (design_info[35] << 8) | design_info[34];

        printf("Design Version     : %u\n", design_ver);
        printf("Back Level Version : %u\n", back_level);
    } else {
        printf("Design Info        : Error (0x%04X)\n", status);
    }
}

/**
 * @brief Command 'sys_iap'
 * Usage: sys_iap <spi_addr|spi_idx> [mode]
 * Trigger In-Application Programming (IAP) update from SPI Flash.
 */
void sys_iap_handler(int nb_params, char **params)
{
    char *c;
    uint32_t spi_target;
    uint8_t mode = MSS_SYS_IAP_PROGRAM_BY_SPIADDR_CMD;

    if (nb_params < 1) {
        printf("Usage: sys_iap <spi_addr|spi_idx> [program|verify|autoupdate]\n");
        return;
    }

    spi_target = (uint32_t)strtoul(params[0], &c, 0);
    if (*c != 0) {
        printf("Error: Invalid SPI target address/index\n");
        return;
    }

    if (nb_params >= 2) {
        if (strcmp(params[1], "verify") == 0) {
            mode = MSS_SYS_IAP_VERIFY_BY_SPIADDR_CMD;
        } else if (strcmp(params[1], "autoupdate") == 0) {
            mode = MSS_SYS_IAP_AUTOUPDATE_CMD;
        }
    }

    printf("Executing IAP Service (Mode 0x%02X, SPI Target: 0x%08X)...\n", mode, spi_target);
    MSS_SYS_select_service_mode(MSS_SYS_SERVICE_POLLING_MODE, NULL);
    uint16_t status = MSS_SYS_execute_iap(mode, spi_target, 0);

    if (status == MSS_SYS_SUCCESS) {
        printf("IAP Operation Succeeded! System reboot recommended.\n");
    } else {
        printf("Error: IAP Failed (Status Code: 0x%04X)\n", status);
    }
}

/**
 * @brief Command 'sys_digest'
 * Usage: sys_digest [options_mask]
 * Recalculates and verifies digests across non-volatile memories (Fabric, SNVM, ENVM, etc.).
 */
void sys_digest_handler(int nb_params, char **params)
{
    uint32_t options = MSS_SYS_DIGEST_CHECK_FABRIC | MSS_SYS_DIGEST_CHECK_SNVM | MSS_SYS_DIGEST_CHECK_ENVM;
    uint8_t digest_err[4] = {0};

    if (nb_params >= 1) {
        options = (uint32_t)strtoul(params[0], NULL, 0);
    }

    printf("Executing Digest Check (Mask: 0x%08X)...\n", options);
    MSS_SYS_select_service_mode(MSS_SYS_SERVICE_POLLING_MODE, NULL);
    uint16_t status = MSS_SYS_digest_check(options, digest_err, 0);

    if (status == MSS_SYS_SUCCESS) {
        printf("Digest Verification PASSED across all checked segments.\n");
    } else {
        printf("Error: Digest Check Failed (Status Code: 0x%04X)\n", status);
        printf("Digest Error Mask: 0x%02X%02X%02X%02X\n", digest_err[3], digest_err[2], digest_err[1], digest_err[0]);
    }
}