/*******************************************************************************
 * Copyright 2019 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * @file e51.c
 * @brief PolarFire SoC Discovery Kit Ethernet Bring-up & Diagnostic Application
 * Target: MPFS-DISCO-KIT (GEM0 + Microchip VSC8221 SGMII PHY @ MDIO Addr 11)
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "mpfs_hal/mss_hal.h"
#include "mpfs_hal/common/nwc/mss_nwc_init.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_registers.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac_sw_cfg.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac_regs.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac.h"
#include "drivers/mss/mss_ethernet_mac/phy.h"
#include "inc/common.h"

int main(void);
#ifndef TARGET_DISCOVERY_KIT
#define TARGET_DISCOVERY_KIT
#endif

#ifndef TARGET_G5_SOC
#define TARGET_G5_SOC
#endif

#define DEMO_UART       &g_mss_uart0_lo
#define PRINT_STRING(x) MSS_UART_polled_tx_string(DEMO_UART, (uint8_t *)x);

/* Forward Declarations */
void msgmii_autonegotiate(const mss_mac_instance_t *this_mac);
extern void dump_vsc8221_regs(const mss_mac_instance_t *this_mac);

/* Buffer & Instance Allocations */
static uint8_t g_mac_rx_buffer[MSS_MAC_RX_RING_SIZE][MSS_MAC_MAX_RX_BUF_SIZE] __attribute__((aligned(16)));
mss_mac_cfg_t g_mac_config;
mss_mac_instance_t *g_test_mac = &g_mac0;

uint8_t tx_pak_arp[128] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFC, 0x00, 0x12, 0x34, 0x58, 0x08, 0x06, 0x00, 0x01,
    0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x00, 0xFC, 0x00, 0x12, 0x34, 0x58, 0xC0, 0xA8, 0x14, 0x6B,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static volatile uint64_t tx_count = 0;
static volatile uint64_t rx_count = 0;
volatile uint32_t g_crc = 0;
volatile int g_loopback = 0;
volatile int g_phy_dump = 0;

const uint8_t *speed_strings[7] = {
    "Autonegotiate", "10M Half Duplex", "10M Full Duplex",
    "100M Half Duplex", "100M Full Duplex", "1000M Half Duplex", "1000M Full Duplex"
};

#define PACKET_IDLE  0
#define PACKET_ARMED 1
#define PACKET_DONE  2
#define PACKET_MAX   16384U

volatile int g_capture = PACKET_IDLE;
uint8_t g_packet_data[PACKET_MAX];
volatile uint32_t g_packet_length = 0;
volatile uint64_t g_tick_counter = 0;
volatile uint8_t g_test_linkup = 0;
uint8_t g_test_fullduplex = 0;
mss_mac_speed_t g_test_speed = MSS_MAC_1000MBPS;
uint64_t link_status_timer = 0;

static void packet_tx_complete_handler(void *this_mac, uint32_t queue_no, mss_mac_tx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)this_mac; (void)queue_no;
    tx_count++;
}

static void mac_rx_callback(void *this_mac, uint32_t queue_no, uint8_t *p_rx_packet, uint32_t pckt_length, mss_mac_rx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)queue_no;

    if (PACKET_ARMED == g_capture) {
        if (pckt_length > PACKET_MAX) pckt_length = PACKET_MAX;
        memcpy(g_packet_data, p_rx_packet, pckt_length);
        g_packet_length = pckt_length;
        g_capture = PACKET_DONE;
    }

    if (g_loopback) {
        MSS_MAC_send_pkt((mss_mac_instance_t *)this_mac, 0, p_rx_packet, pckt_length | g_crc, NULL);
    }

    MSS_MAC_receive_pkt((mss_mac_instance_t *)this_mac, 0, p_rx_packet, 0, 1);
    rx_count++;
}

static void low_level_init(void) {
    uint32_t count;

    MSS_MAC_cfg_struct_def_init(&g_mac_config);

    g_test_mac = &g_mac0;
    g_mac_config.speed_duplex_select = MSS_MAC_ANEG_ALL_SPEEDS;
    g_mac_config.mac_addr[0] = 0x00;
    g_mac_config.mac_addr[1] = 0xFC;
    g_mac_config.mac_addr[2] = 0x00;
    g_mac_config.mac_addr[3] = 0x12;
    g_mac_config.mac_addr[4] = 0x34;
    g_mac_config.mac_addr[5] = 0x58; /* Discovery Kit Default */

    g_mac_config.tsu_clock_select = 1U;
    g_mac_config.phy_addr = PHY_VSC8221_MDIO_ADDR; /* 11U */
    g_mac_config.phy_type = MSS_MAC_DEV_PHY_VSC8221;
    g_mac_config.phy_flags = PHY_VSC8221_EEPROM_INIT;
    g_mac_config.pcs_phy_addr = SGMII_MDIO_ADDR; /* 16U */
    g_mac_config.interface_type = TBI;

    g_mac_config.phy_autonegotiate = MSS_MAC_VSC8221_phy_autonegotiate;
    g_mac_config.phy_mac_autonegotiate = MSS_MAC_VSC8221_mac_autonegotiate;
    g_mac_config.phy_get_link_status = MSS_MAC_VSC8221_phy_get_link_status;
    g_mac_config.phy_init = MSS_MAC_VSC8221_phy_init;
    g_mac_config.phy_set_link_speed = MSS_MAC_VSC8221_phy_set_link_speed;

    MSS_MAC_init(g_test_mac, &g_mac_config);

    g_test_mac->mac_base->NETWORK_CONTROL &= ~GEM_ENABLE_TRANSMIT;
    g_test_mac->mac_base->TX_Q_SEG_ALLOC_Q0TO3 = 2;
    g_test_mac->mac_base->NETWORK_CONTROL |= GEM_ENABLE_TRANSMIT;

    MSS_MAC_set_tx_callback(g_test_mac, 0, packet_tx_complete_handler);
    MSS_MAC_set_rx_callback(g_test_mac, 0, mac_rx_callback);

    for (count = 0; count < MSS_MAC_RX_RING_SIZE; ++count) {
        if (count != (MSS_MAC_RX_RING_SIZE - 1)) {
            MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, 0);
        } else {
            MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, -1);
        }
    }
}

void prvLinkStatusTask(void) {
    if (g_tick_counter >= link_status_timer) {
        g_test_linkup = MSS_MAC_get_link_status(g_test_mac, &g_test_speed, &g_test_fullduplex);
        link_status_timer = g_tick_counter + 250;
    }
}

void E51_sysTick_IRQHandler(void) {
    g_tick_counter += HART0_TICK_RATE_MS;
}

static void print_help(void) {
    char info[200];
    PRINT_STRING("\r\n=== PolarFire SoC Discovery Kit Ethernet Diagnostics ===\r\n");
    PRINT_STRING("a - Initiate PHY Media autonegotiation\r\n");
    PRINT_STRING("A - Initiate PHY SGMII autonegotiation\r\n");
    PRINT_STRING("b - Toggle MAC Local Loopback\r\n");
    PRINT_STRING("c - Capture and dump next received packet\r\n");
    PRINT_STRING("e - Dump SGMII & PCS Registers\r\n");
    PRINT_STRING("h - Display this help menu\r\n");
    PRINT_STRING("l - Toggle Software Loopback\r\n");
    PRINT_STRING("r - Reset MAC statistics\r\n");
    PRINT_STRING("s - Display link status & MAC statistics\r\n");
    PRINT_STRING("S - Initiate MAC SGMII autonegotiation\r\n");
    PRINT_STRING("t - Transmit sample ARP frame\r\n");
    PRINT_STRING("v - Toggle GEM PCS SGMII Loopback\r\n");
    PRINT_STRING("P - Toggle VSC8221 PHY Loopback\r\n");
    PRINT_STRING("x - Toggle PHY register dump on stats command\r\n");
    PRINT_STRING("2-8 - Change link speed mode\r\n\r\n");
}

void stats_dump(void) {
    char info[200];
    if (g_phy_dump) {
        dump_vsc8221_regs(g_test_mac);
    }
    sprintf(info, "Packets TX: %lu | RX: %lu\r\n", (unsigned long)tx_count, (unsigned long)rx_count);
    PRINT_STRING(info);
}

void packet_dump(void) {
    char info[200], temp[10];
    uint32_t dump_addr = 0;
    g_capture = PACKET_IDLE;

    sprintf(info, "\r\n--- %d Byte Packet Captured ---\r\n", g_packet_length);
    PRINT_STRING(info);

    while ((g_packet_length - dump_addr) >= 16) {
        sprintf(info, "%04X: ", dump_addr);
        for (int i = 0; i < 16; i++) {
            sprintf(temp, "%02X ", g_packet_data[dump_addr + i]);
            strcat(info, temp);
        }
        strcat(info, " | ");
        for (int i = 0; i < 16; i++) {
            uint8_t c = g_packet_data[dump_addr + i];
            sprintf(temp, "%c", (c >= 32 && c < 127) ? c : '.');
            strcat(info, temp);
        }
        strcat(info, "\r\n");
        PRINT_STRING(info);
        dump_addr += 16;
    }
}

void mac_task(void *pvParameters) {
    (void)pvParameters;
    char info[200];
    uint8_t rx_buff[1];
    size_t rx_size;

    SYSREG->SOFT_RESET_CR = 0U;
    SYSREG->SUBBLK_CLOCK_CR = 0xFFFFFFFFUL;

    __disable_local_irq((int8_t)MMUART0_E51_INT);
    SysTick_Config();

    MSS_UART_init(DEMO_UART, MSS_UART_115200_BAUD, MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
    PRINT_STRING("\r\n***** PolarFire SoC Discovery Kit Ethernet Diagnostics *****\r\n");
    __enable_irq();

    low_level_init();
    print_help();

    while (1) {
        prvLinkStatusTask();

        if (PACKET_DONE == g_capture) {
            packet_dump();
        }

        rx_size = MSS_UART_get_rx(DEMO_UART, rx_buff, sizeof(rx_buff));
        if (rx_size > 0) {
            char cmd = rx_buff[0];

            if (cmd == 'a') {
                PRINT_STRING("Starting PHY Media Autonegotiation...\r\n");
                g_test_mac->phy_autonegotiate(g_test_mac);
                PRINT_STRING("Done.\r\n");
            } else if (cmd == 'A') {
                PRINT_STRING("Starting SGMII Autonegotiation...\r\n");
                g_test_mac->phy_mac_autonegotiate(g_test_mac);
                PRINT_STRING("Done.\r\n");
            }


           /*  else if (cmd == 'b') {
                uint32_t ctrl = g_test_mac->mac_base->NETWORK_CONTROL;
                if (ctrl & GEM_LOOPBACK_LOCAL) {
                    g_test_mac->mac_base->NETWORK_CONTROL &= ~GEM_LOOPBACK_LOCAL;
                    PRINT_STRING("MAC Loopback Disabled\r\n");
                } else {
                    g_test_mac->mac_base->NETWORK_CONTROL |= GEM_LOOPBACK_LOCAL;
                    PRINT_STRING("MAC Loopback Enabled\r\n");
                }
          */
            else if (cmd == 'b') {
                /* Note: TX and RX must be disabled while toggling GEM loopback mode */
                if (0 == (g_test_mac->mac_base->NETWORK_CONTROL & GEM_LOOPBACK_LOCAL)) {
                    /* 1. Stop Transmit and Receive */
                    g_test_mac->mac_base->NETWORK_CONTROL &= ~(GEM_ENABLE_TRANSMIT | GEM_ENABLE_RECEIVE);

                    /* 2. Enable GEM Local Loopback Bits */
                    g_test_mac->mac_base->NETWORK_CONTROL |= (GEM_LOOPBACK_LOCAL | GEM_LOOPBACK);

                    /* 3. Re-enable Transmit and Receive */
                    g_test_mac->mac_base->NETWORK_CONTROL |= (GEM_ENABLE_TRANSMIT | GEM_ENABLE_RECEIVE);

                    PRINT_STRING("Hardware MAC Loopback Enabled\r\n");
                } else {
                    /* 1. Stop Transmit and Receive */
                    g_test_mac->mac_base->NETWORK_CONTROL &= ~(GEM_ENABLE_TRANSMIT | GEM_ENABLE_RECEIVE);

                    /* 2. Clear GEM Local Loopback Bits */
                    g_test_mac->mac_base->NETWORK_CONTROL &= ~(GEM_LOOPBACK_LOCAL | GEM_LOOPBACK);

                    /* 3. Re-enable Transmit and Receive */
                    g_test_mac->mac_base->NETWORK_CONTROL |= (GEM_ENABLE_TRANSMIT | GEM_ENABLE_RECEIVE);

                    PRINT_STRING("Hardware MAC Loopback Disabled\r\n");
                }

            } else if (cmd == 'c') {
                PRINT_STRING("Packet Capture Armed...\r\n");
                g_capture = PACKET_ARMED;
            } else if (cmd == 'e') {
                sprintf(info, "PCS Control: 0x%08X | PCS Status: 0x%08X\r\n",
                        (unsigned int)g_test_mac->mac_base->PCS_CONTROL,
                        (unsigned int)g_test_mac->mac_base->PCS_STATUS);
                PRINT_STRING(info);
            } else if (cmd == 'h') {
                print_help();
            } else if (cmd == 'l') {
                g_loopback = !g_loopback;
                sprintf(info, "Software Loopback %s\r\n", g_loopback ? "Enabled" : "Disabled");
                PRINT_STRING(info);
            } else if (cmd == 'r') {
                MSS_MAC_clear_statistics(g_test_mac);
                tx_count = 0; rx_count = 0;
                PRINT_STRING("Statistics Cleared.\r\n");
            } else if (cmd == 's') {
                sprintf(info, "Link: %s | Speed: %s | Duplex: %s\r\n",
                        g_test_linkup ? "UP" : "DOWN",
                        (g_test_speed == MSS_MAC_1000MBPS) ? "1Gbps" : "100Mbps",
                        g_test_fullduplex ? "Full" : "Half");
                PRINT_STRING(info);
                stats_dump();
            } else if (cmd == 'S') {
                msgmii_autonegotiate(g_test_mac);
                PRINT_STRING("MAC SGMII Autonegotiation triggered.\r\n");
            } else if (cmd == 't') {
                memcpy(&tx_pak_arp[6], g_test_mac->mac_addr, 6);
                int32_t st = MSS_MAC_send_pkt(g_test_mac, 0, tx_pak_arp, sizeof(tx_pak_arp) | g_crc, NULL);
                sprintf(info, "ARP Transmit Status: %d\r\n", st);
                PRINT_STRING(info);
            } else if (cmd == 'x') {
                g_phy_dump = !g_phy_dump;
                sprintf(info, "PHY Dump %s\r\n", g_phy_dump ? "Enabled" : "Disabled");
                PRINT_STRING(info);
            } else if (cmd >= '2' && cmd <= '8') {
                mss_mac_speed_mode_t spd = (mss_mac_speed_mode_t)(cmd - '2');
                MSS_MAC_change_speed(g_test_mac, g_test_mac->speed_duplex_select, spd);
                sprintf(info, "Speed Mode changed to: %s\r\n", speed_strings[spd]);
                PRINT_STRING(info);
            }
            /* Restored Commands for SGMII Loopback Testing */
                        else if (cmd == 'v') {
                            volatile uint32_t pcs_ctrl = g_test_mac->mac_base->PCS_CONTROL;
                            pcs_ctrl ^= GEM_LOOPBACK_MODE;
                            g_test_mac->mac_base->PCS_CONTROL = pcs_ctrl;

                            if (pcs_ctrl & GEM_LOOPBACK_MODE) {
                                PRINT_STRING("PCS Loopback Mode Enabled\r\n");
                            } else {
                                PRINT_STRING("PCS Loopback Mode Disabled\r\n");
                            }
                        }
                        else if (cmd == 'P') {
                            static uint16_t phy_loopback_state = 0;
                            uint8_t phy_addr = (uint8_t)g_test_mac->phy_addr;

                            if (phy_loopback_state == 0) {
                                /* Enable Near-End PHY Loopback */
                                MSS_MAC_write_phy_reg(g_test_mac, phy_addr, 31, 0x0000U);
                                uint16_t bmcr = MSS_MAC_read_phy_reg(g_test_mac, phy_addr, MII_BMCR);
                                MSS_MAC_write_phy_reg(g_test_mac, phy_addr, MII_BMCR, bmcr | BMCR_LOOPBACK);
                                phy_loopback_state = 1;
                                PRINT_STRING("PHY Near-End Loopback Enabled\r\n");
                            } else {
                                /* Disable Near-End PHY Loopback */
                                MSS_MAC_write_phy_reg(g_test_mac, phy_addr, 31, 0x0000U);
                                uint16_t bmcr = MSS_MAC_read_phy_reg(g_test_mac, phy_addr, MII_BMCR);
                                MSS_MAC_write_phy_reg(g_test_mac, phy_addr, MII_BMCR, bmcr & ~BMCR_LOOPBACK);
                                phy_loopback_state = 0;
                                PRINT_STRING("PHY Loopback Disabled\r\n");
                            }
                        }
                        else {
                MSS_UART_polled_tx(DEMO_UART, rx_buff, 1);
            }
        }
    }
}

void e51(void) {
    write_csr(mscratch, 0);
    write_csr(mcause, 0);
    write_csr(mepc, 0);
    PLIC_init();
    //mac_task(0);
    main();
}
