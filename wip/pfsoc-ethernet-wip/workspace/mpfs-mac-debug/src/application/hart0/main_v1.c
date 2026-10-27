/*******************************************************************************
 * PolarFire SoC Discovery Kit - Step 1: Full-Text CLI & Packet Loopback Test
 * Core Target: GEM0 + VSC8221 SGMII PHY (MDIO Address 11)
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "mpfs_hal/mss_hal.h"
#include "mpfs_hal/common/nwc/mss_nwc_init.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_registers.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac_sw_cfg.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac_regs.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac.h"
#include "drivers/mss/mss_ethernet_mac/phy.h"

#ifndef TARGET_DISCOVERY_KIT
#define TARGET_DISCOVERY_KIT
#endif

#ifndef TARGET_G5_SOC
#define TARGET_G5_SOC
#endif

#define DEMO_UART       &g_mss_uart0_lo
#define PRINT_STRING(x) MSS_UART_polled_tx_string(DEMO_UART, (const uint8_t *)x);

#define PACKET_IDLE  0
#define PACKET_ARMED 1
#define PACKET_DONE  2
#define PACKET_MAX   1518U

/* Forward Declarations */
void mac_task(void *pvParameters);
void e51(void);
int main(void);

/* Buffers & Instance Declarations */
static uint8_t g_mac_rx_buffer[MSS_MAC_RX_RING_SIZE][MSS_MAC_MAX_RX_BUF_SIZE] __attribute__((aligned(16)));
static mss_mac_cfg_t g_mac_config;
mss_mac_instance_t *g_test_mac = &g_mac0;

/* Sample 60-byte ARP Request Packet */
uint8_t tx_pak_arp[60] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* Broadcast Destination MAC */
    0x00, 0xFC, 0x00, 0x12, 0x34, 0x58,  /* Source MAC Address */
    0x08, 0x06,                          /* EtherType: ARP */
    0x00, 0x01,                          /* Hardware Type: Ethernet */
    0x08, 0x00,                          /* Protocol Type: IPv4 */
    0x06, 0x04,                          /* HW Size: 6, Proto Size: 4 */
    0x00, 0x01,                          /* Opcode: Request */
    0x00, 0xFC, 0x00, 0x12, 0x34, 0x58,  /* Sender MAC */
    0xC0, 0xA8, 0x14, 0x6B,              /* Sender IP: 192.168.20.107 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Target MAC */
    0xC0, 0xA8, 0x14, 0x01               /* Target IP: 192.168.20.1 */
};

/* Global Diagnostic State Variables */
static volatile uint64_t g_tx_count = 0;
static volatile uint64_t g_rx_count = 0;
static volatile int g_capture = PACKET_IDLE;
static uint8_t g_packet_data[PACKET_MAX];
static volatile uint32_t g_packet_length = 0;
static volatile uint64_t g_tick_counter = 0;
static uint64_t g_link_status_timer = 0;
static volatile uint8_t g_test_linkup = 0;
static uint8_t g_test_fullduplex = 0;
static mss_mac_speed_t g_test_speed = MSS_MAC_1000MBPS;
static bool g_auto_rearm = false;

/* Interrupt & Task Handler Routines */
void E51_sysTick_IRQHandler(void) {
    g_tick_counter += HART0_TICK_RATE_MS;
}

static void prvLinkStatusTask(void) {
    if (g_tick_counter >= g_link_status_timer) {
        g_test_linkup = MSS_MAC_get_link_status(g_test_mac, &g_test_speed, &g_test_fullduplex);
        g_link_status_timer = g_tick_counter + 250;
    }
}

static void packet_tx_complete_handler(void *this_mac, uint32_t queue_no, mss_mac_tx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)this_mac; (void)queue_no;
    g_tx_count++;
}

static void mac_rx_callback(void *this_mac, uint32_t queue_no, uint8_t *p_rx_packet, uint32_t pckt_length, mss_mac_rx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)queue_no;

    /* If packet capture is armed, store a copy of the packet */
    if (PACKET_ARMED == g_capture) {
        if (pckt_length > PACKET_MAX) pckt_length = PACKET_MAX;
        memcpy(g_packet_data, p_rx_packet, pckt_length);
        g_packet_length = pckt_length;
        g_capture = PACKET_DONE;
    }

    MSS_MAC_receive_pkt((mss_mac_instance_t *)this_mac, 0, p_rx_packet, 0, 1);
    g_rx_count++;
}

static void low_level_init(void) {
    MSS_MAC_cfg_struct_def_init(&g_mac_config);

    g_test_mac = &g_mac0;
    g_mac_config.speed_duplex_select = MSS_MAC_ANEG_ALL_SPEEDS;
    g_mac_config.mac_addr[0] = 0x00;
    g_mac_config.mac_addr[1] = 0xFC;
    g_mac_config.mac_addr[2] = 0x00;
    g_mac_config.mac_addr[3] = 0x12;
    g_mac_config.mac_addr[4] = 0x34;
    g_mac_config.mac_addr[5] = 0x58;

    g_mac_config.tsu_clock_select = 1U;
    g_mac_config.phy_addr = PHY_VSC8221_MDIO_ADDR; /* Addr 11 */
    g_mac_config.phy_type = MSS_MAC_DEV_PHY_VSC8221;
    g_mac_config.phy_flags = PHY_VSC8221_EEPROM_INIT;
    g_mac_config.pcs_phy_addr = SGMII_MDIO_ADDR;   /* Addr 16 */
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

    /* Populate the RX descriptor ring */
    for (uint32_t count = 0; count < MSS_MAC_RX_RING_SIZE; ++count) {
        if (count != (MSS_MAC_RX_RING_SIZE - 1)) {
            MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, 0);
        } else {
            MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, -1);
        }
    }
}

static void print_help(void) {
    PRINT_STRING("\r\n================ Step 1: GEM0 Full-Text CLI ================\r\n");
    PRINT_STRING("  loopback pcs  - Toggle GEM PCS SGMII Loopback Mode\r\n");
    PRINT_STRING("  capture       - Arm for a single-packet capture\r\n");
    PRINT_STRING("  monitor       - Toggle continuous packet monitoring (stream)\r\n");
    PRINT_STRING("  tx arp        - Transmit Sample ARP Frame\r\n");
    PRINT_STRING("  status        - Display Link Status & Frame Counters\r\n");
    PRINT_STRING("  help          - Print Help Menu\r\n");
    PRINT_STRING("============================================================\r\n\r\n");
}

/* Single Unified Packet Dump Function */
static void packet_dump(void) {
    char info[200], temp[10];
    uint32_t dump_addr = 0;

    sprintf(info, "\r\n--- %u Byte Packet Captured ---\r\n", (unsigned int)g_packet_length);
    PRINT_STRING(info);

    while ((g_packet_length - dump_addr) >= 16) {
        sprintf(info, "0x%04X: ", (unsigned int)dump_addr);
        for (uint32_t i = 0; i < 16; i++) {
            sprintf(temp, "%02X ", g_packet_data[dump_addr + i]);
            strcat(info, temp);
        }
        strcat(info, " | ");
        for (uint32_t i = 0; i < 16; i++) {
            uint8_t c = g_packet_data[dump_addr + i];
            sprintf(temp, "%c", (c >= 32 && c < 127) ? c : '.');
            strcat(info, temp);
        }
        strcat(info, "\r\n");
        PRINT_STRING(info);
        dump_addr += 16;
    }

    if ((g_packet_length - dump_addr) > 0) {
        sprintf(info, "0x%04X: ", (unsigned int)dump_addr);
        for (uint32_t i = 0; i < (g_packet_length - dump_addr); i++) {
            sprintf(temp, "%02X ", g_packet_data[dump_addr + i]);
            strcat(info, temp);
        }
        strcat(info, "\r\n");
        PRINT_STRING(info);
    }
    PRINT_STRING("\r\n");

    /* Handle Capture State Re-arm */
    if (g_auto_rearm) {
        g_capture = PACKET_ARMED;
    } else {
        g_capture = PACKET_IDLE;
    }
}

/* Character Reader with Backspace & Terminal Echo */
static bool get_cli_line(char *line_buf, size_t max_len) {
    static size_t idx = 0;
    uint8_t rx_byte = 0;

    if (MSS_UART_get_rx(DEMO_UART, &rx_byte, 1) > 0) {
        /* Handle Enter key */
        if (rx_byte == '\r' || rx_byte == '\n') {
            PRINT_STRING("\r\n");
            line_buf[idx] = '\0';
            idx = 0;
            return true;
        }
        /* Handle Backspace (0x08) or Delete (0x7F) */
        else if (rx_byte == '\b' || rx_byte == 0x7F) {
            if (idx > 0) {
                idx--;
                PRINT_STRING("\b \b"); /* Erase character on terminal */
            }
        }
        /* Handle Printable Characters */
        else if (idx < max_len - 1 && rx_byte >= 32 && rx_byte <= 126) {
            uint8_t echo[2] = {rx_byte, 0};
            PRINT_STRING(echo);
            line_buf[idx++] = (char)rx_byte;
        }
    }
    return false;
}

void mac_task(void *pvParameters) {
    (void)pvParameters;
    char info[200];
    char cli_input[128];

    SYSREG->SOFT_RESET_CR = 0U;
    SYSREG->SUBBLK_CLOCK_CR = 0xFFFFFFFFUL;

    __disable_local_irq((int8_t)MMUART0_E51_INT);
    SysTick_Config();

    MSS_UART_init(DEMO_UART, MSS_UART_115200_BAUD, MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
    PRINT_STRING("\r\n***** PolarFire SoC Discovery Kit - Step 1 Test *****\r\n");
    __enable_irq();

    low_level_init();
    print_help();

    PRINT_STRING("eth-disco> ");

    while (1) {
        prvLinkStatusTask();

        /* Print dumped packet when RX callback signals completion */
        if (PACKET_DONE == g_capture) {
            packet_dump();
            PRINT_STRING("eth-disco> ");
        }

        if (get_cli_line(cli_input, sizeof(cli_input))) {
            char *cmd = cli_input;
            while (*cmd == ' ') cmd++; /* Trim leading spaces */

            if (strcmp(cmd, "loopback pcs") == 0 || strcmp(cmd, "pcs loopback") == 0) {
                volatile uint32_t pcs_ctrl = g_test_mac->mac_base->PCS_CONTROL;
                pcs_ctrl ^= GEM_LOOPBACK_MODE;
                g_test_mac->mac_base->PCS_CONTROL = pcs_ctrl;

                if (pcs_ctrl & GEM_LOOPBACK_MODE) {
                    PRINT_STRING("[CMD] GEM PCS SGMII Loopback Enabled\r\n");
                } else {
                    PRINT_STRING("[CMD] GEM PCS SGMII Loopback Disabled\r\n");
                }
            }
            /* Single-packet capture (One-shot) */
            else if (strcmp(cmd, "capture") == 0 || strcmp(cmd, "capture single") == 0) {
                g_auto_rearm = false;
                g_capture = PACKET_ARMED;
                PRINT_STRING("[CMD] Single-Packet Capture Armed...\r\n");
            }
            /* Continuous packet capture toggle */
            else if (strcmp(cmd, "monitor") == 0 || strcmp(cmd, "capture stream") == 0) {
                g_auto_rearm = !g_auto_rearm;
                if (g_auto_rearm) {
                    g_capture = PACKET_ARMED;
                    PRINT_STRING("[CMD] Continuous Packet Monitoring ENABLED\r\n");
                } else {
                    g_capture = PACKET_IDLE;
                    PRINT_STRING("[CMD] Continuous Packet Monitoring DISABLED\r\n");
                }
            }
            else if (strcmp(cmd, "tx arp") == 0 || strcmp(cmd, "tx") == 0) {
                memcpy(&tx_pak_arp[6], g_test_mac->mac_addr, 6);
                int32_t st = MSS_MAC_send_pkt(g_test_mac, 0, tx_pak_arp, sizeof(tx_pak_arp), NULL);
                sprintf(info, "[CMD] Transmitted ARP Packet. Driver Status: %d\r\n", (int)st);
                PRINT_STRING(info);
            }
            else if (strcmp(cmd, "status") == 0) {
                sprintf(info, "[STATUS] Link: %s | Speed: %s | Duplex: %s\r\n",
                        g_test_linkup ? "UP" : "DOWN",
                        (g_test_speed == MSS_MAC_1000MBPS) ? "1Gbps" : "100Mbps",
                        g_test_fullduplex ? "Full" : "Half");
                PRINT_STRING(info);
                sprintf(info, "[STATUS] Transmitted Packets: %lu | Received Packets: %lu\r\n",
                        (unsigned long)g_tx_count, (unsigned long)g_rx_count);
                PRINT_STRING(info);
            }
            else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
                print_help();
            }
            else if (strlen(cmd) > 0) {
                sprintf(info, "Unknown command: '%s'. Type 'help' for options.\r\n", cmd);
                PRINT_STRING(info);
            }

            PRINT_STRING("eth-disco> ");
        }
    }
}

void e51(void) {
    write_csr(mscratch, 0);
    write_csr(mcause, 0);
    write_csr(mepc, 0);
    PLIC_init();
    mac_task(0);
}

int main(void) {
    e51();
    return 0;
}
