/*******************************************************************************
 * PolarFire SoC Discovery Kit - Step 3: Complete Packet Diagnostics & Protocol Suite
 * Target: MPFS GEM0 + VSC8221 SGMII PHY (MDIO Address 11)
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

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
#define ETH_MIN_FRAME_LEN 64U

/* Forward Declarations */
void mac_task(void *pvParameters);
void e51(void);
int main(void);
extern void dump_vsc8221_regs(const mss_mac_instance_t *this_mac);

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

static uint8_t g_last_tx_buf[PACKET_MAX] __attribute__((aligned(8)));
static uint32_t g_last_tx_len = 0;

/* Global State Variables */
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
static uint8_t g_phy_addr = PHY_VSC8221_MDIO_ADDR; /* 11U */
static uint8_t g_board_ip[4] = {192, 168, 20, 107};

static uint32_t gem_read_reg(uint32_t offset) {
    return *(volatile uint32_t *)((uintptr_t)g_test_mac->mac_base + offset);
}

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
    g_mac_config.phy_addr = g_phy_addr;
    g_mac_config.phy_type = MSS_MAC_DEV_PHY_VSC8221;
    g_mac_config.phy_flags = PHY_VSC8221_EEPROM_INIT;
    g_mac_config.pcs_phy_addr = SGMII_MDIO_ADDR;
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

    for (uint32_t count = 0; count < MSS_MAC_RX_RING_SIZE; ++count) {
        if (count != (MSS_MAC_RX_RING_SIZE - 1)) {
            MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, 0);
        } else {
            MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, -1);
        }
    }
}

static uint16_t calculate_ip_checksum(const uint8_t *hdr, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i += 2) {
        sum += (uint32_t)((hdr[i] << 8) | (i + 1 < len ? hdr[i + 1] : 0));
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static int send_ethernet_frame(const uint8_t *buf, uint32_t len) {
    if (len > PACKET_MAX) len = PACKET_MAX;
    memcpy(g_last_tx_buf, buf, len);
    g_last_tx_len = len;

    int32_t st = MSS_MAC_send_pkt(g_test_mac, 0, g_last_tx_buf, len, NULL);
    return (int)st;
}

static void print_help(void) {
    PRINT_STRING("\r\n================ Step 3: Complete Diagnostics CLI ================\r\n");
    PRINT_STRING("  loopback pcs                  - Toggle GEM PCS SGMII Loopback\r\n");
    PRINT_STRING("  loopback phy                  - Toggle VSC8221 PHY Loopback\r\n");
    PRINT_STRING("  loopback off                  - Disable  VSC8221 PHY Loopback\r\n");
    PRINT_STRING("  capture                       - Arm single-packet capture\r\n");
    PRINT_STRING("  monitor                       - Toggle continuous packet monitoring\r\n");
    PRINT_STRING("  rx inspect                    - Detailed layer-by-layer frame decoder\r\n");
    PRINT_STRING("  packet diff [bytes]           - Side-by-side TX vs RX buffer comparison\r\n");
    PRINT_STRING("  desc inspect                  - Inspect DMA descriptor queue pointers\r\n");
    PRINT_STRING("  tx arp                        - Transmit sample ARP frame\r\n");
    PRINT_STRING("  tx udp <ip> <sp> <dp> <msg>   - Transmit raw IPv4 UDP packet\r\n");
    PRINT_STRING("  tx tcp <ip> <sp> <dp> <flg> <m> - Transmit raw IPv4 TCP packet\r\n");
    PRINT_STRING("  dhcp                          - Broadcast DHCP Discover packet\r\n");
    PRINT_STRING("  status                        - Display link & packet counters\r\n");
    PRINT_STRING("  mdio scan                     - Scan MDIO bus (0-31) for PHYs\r\n");
    PRINT_STRING("  mdio read <reg_hex>           - Read 16-bit PHY register\r\n");
    PRINT_STRING("  mdio write <r> <v>            - Write 16-bit PHY register\r\n");
    PRINT_STRING("  phy dump                      - Dump PHY registers (0x00-0x0F)\r\n");
    PRINT_STRING("  gem dump                      - Dump GEM0 MAC registers (0x000-0x084)\r\n");
    PRINT_STRING("  help                          - Print Help Menu\r\n");
    PRINT_STRING("===================================================================\r\n\r\n");
}

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

    if (g_auto_rearm) {
        g_capture = PACKET_ARMED;
    } else {
        g_capture = PACKET_IDLE;
    }
}

static void inspect_rx_packet(void) {
    char info[200];
    uint32_t len = g_packet_length;
    uint8_t *buf = g_packet_data;

    if (len == 0) {
        PRINT_STRING("\r\n[Inspector] No packet currently available in RX buffer.\r\n\r\n");
        return;
    }

    PRINT_STRING("\r\n==================== RX PACKET INSPECTOR ====================\r\n");
    sprintf(info, "Total Length Captured: %u bytes\r\n", (unsigned int)len);
    PRINT_STRING(info);

    if (len >= 14) {
        sprintf(info, "Dst MAC   : %02X:%02X:%02X:%02X:%02X:%02X\r\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
        PRINT_STRING(info);
        sprintf(info, "Src MAC   : %02X:%02X:%02X:%02X:%02X:%02X\r\n", buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
        PRINT_STRING(info);

        uint16_t ethertype = (uint16_t)((buf[12] << 8) | buf[13]);
        if (ethertype == 0x0800) {
            PRINT_STRING("EtherType : 0x0800 (IPv4)\r\n");
            if (len >= 34) {
                uint8_t protocol = buf[23];
                sprintf(info, "  |- Src IP  : %u.%u.%u.%u\r\n", buf[26], buf[27], buf[28], buf[29]); PRINT_STRING(info);
                sprintf(info, "  |- Dst IP  : %u.%u.%u.%u\r\n", buf[30], buf[31], buf[32], buf[33]); PRINT_STRING(info);

                if (protocol == 17) {
                    PRINT_STRING("  \\- Protocol: UDP (17)\r\n");
                    if (len >= 42) {
                        sprintf(info, "     |- Src Port : %u\r\n", (buf[34] << 8) | buf[35]); PRINT_STRING(info);
                        sprintf(info, "     |- Dst Port : %u\r\n", (buf[36] << 8) | buf[37]); PRINT_STRING(info);
                        sprintf(info, "     \\- UDP Len  : %u bytes\r\n", (buf[38] << 8) | buf[39]); PRINT_STRING(info);
                    }
                } else if (protocol == 6) {
                    PRINT_STRING("  \\- Protocol: TCP (6)\r\n");
                    if (len >= 54) {
                        sprintf(info, "     |- Src Port : %u\r\n", (buf[34] << 8) | buf[35]); PRINT_STRING(info);
                        sprintf(info, "     |- Dst Port : %u\r\n", (buf[36] << 8) | buf[37]); PRINT_STRING(info);
                        sprintf(info, "     \\- Flags    : 0x%02X\r\n", buf[47]); PRINT_STRING(info);
                    }
                } else if (protocol == 1) {
                    PRINT_STRING("  \\- Protocol: ICMP Ping (1)\r\n");
                }
            }
        } else if (ethertype == 0x0806) {
            PRINT_STRING("EtherType : 0x0806 (ARP)\r\n");
            if (len >= 42) {
                uint16_t opcode = (uint16_t)((buf[20] << 8) | buf[21]);
                sprintf(info, "  |- Opcode   : %s (%u)\r\n", (opcode == 1) ? "Request" : (opcode == 2) ? "Reply" : "Unknown", opcode); PRINT_STRING(info);
                sprintf(info, "  |- Sender IP: %u.%u.%u.%u\r\n", buf[28], buf[29], buf[30], buf[31]); PRINT_STRING(info);
                sprintf(info, "  \\- Target IP: %u.%u.%u.%u\r\n", buf[38], buf[39], buf[40], buf[41]); PRINT_STRING(info);
            }
        } else {
            sprintf(info, "EtherType : 0x%04X\r\n", ethertype);
            PRINT_STRING(info);
        }
    }
    PRINT_STRING("=============================================================\r\n\r\n");
}

static void compare_tx_rx_buffers(uint32_t len) {
    char info[128];
    if (len > PACKET_MAX) len = PACKET_MAX;
    if (len == 0) len = (g_last_tx_len > 0) ? g_last_tx_len : 64;

    uint32_t mismatches = 0;

    PRINT_STRING("\r\n================ TX / RX Buffer Comparison ================\r\n");
    PRINT_STRING("Offset  | TX Byte | RX Byte | Status\r\n");
    PRINT_STRING("--------+---------+---------+---------\r\n");

    for (uint32_t i = 0; i < len; i++) {
        uint8_t tx_val = g_last_tx_buf[i];
        uint8_t rx_val = g_packet_data[i];
        bool match = (tx_val == rx_val);

        if (!match) mismatches++;

        if (!match || i < 14 || i >= len - 4) {
            sprintf(info, "0x%04X  |   0x%02X  |   0x%02X  | %s\r\n",
                    (unsigned int)i, tx_val, rx_val, match ? "MATCH" : "MISMATCH ***");
            PRINT_STRING(info);
        } else if (i == 14) {
            PRINT_STRING("...     |   ...   |   ...   | [Payload Matching OK]\r\n");
        }
    }

    PRINT_STRING("-----------------------------------------------------------\r\n");
    sprintf(info, "Total Bytes Inspected : %u\r\n", (unsigned int)len); PRINT_STRING(info);
    sprintf(info, "Comparison Result     : %s (%u Mismatches)\r\n",
            (mismatches == 0) ? "PASS" : "FAIL", (unsigned int)mismatches); PRINT_STRING(info);
    PRINT_STRING("===========================================================\r\n\r\n");
}

static void inspect_dma_descriptors(void) {
    char info[128];
    uint32_t rx_q_ptr = gem_read_reg(0x018);
    uint32_t tx_q_ptr = gem_read_reg(0x01C);

    PRINT_STRING("\r\n================ Hard MSS GEM0 DMA Ring Inspection ================\r\n");
    sprintf(info, "RX Descriptor Queue Reg (0x018) : 0x%08X\r\n", (unsigned int)rx_q_ptr); PRINT_STRING(info);
    sprintf(info, "TX Descriptor Queue Reg (0x01C) : 0x%08X\r\n", (unsigned int)tx_q_ptr); PRINT_STRING(info);
    PRINT_STRING("-------------------------------------------------------------------\r\n");
    sprintf(info, "TX Count: %lu | RX Count: %lu\r\n", (unsigned long)g_tx_count, (unsigned long)g_rx_count); PRINT_STRING(info);
    sprintf(info, "Pending Captured Length         : %u Bytes\r\n", (unsigned int)g_packet_length); PRINT_STRING(info);
    PRINT_STRING("===================================================================\r\n\r\n");
}

static void send_udp_packet(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4,
                            uint16_t src_port, uint16_t dst_port, const char *payload) {
    uint8_t frame[PACKET_MAX] = {0};
    uint32_t payload_len = (uint32_t)strlen(payload);
    uint32_t udp_len = 8 + payload_len;
    uint32_t ip_len = 20 + udp_len;
    uint32_t total_len = 14 + ip_len;

    /* 1. Layer 2: Broadcast Destination MAC so all local switch/Wi-Fi ports receive the frame */
    memset(&frame[0], 0xFF, 6);
    memcpy(&frame[6], g_test_mac->mac_addr, 6);
    frame[12] = 0x08; frame[13] = 0x00; /* IPv4 */

    /* 2. Layer 3: IPv4 Header using current assigned g_board_ip */
    frame[14] = 0x45; frame[15] = 0x00;
    frame[16] = (uint8_t)(ip_len >> 8); frame[17] = (uint8_t)(ip_len & 0xFF);
    frame[18] = 0x12; frame[19] = 0x34;
    frame[20] = 0x40; frame[21] = 0x00;
    frame[22] = 64;   frame[23] = 17;   /* UDP Protocol */
    memcpy(&frame[26], g_board_ip, 4);  /* Source IP (from DHCP) */
    frame[30] = ip1; frame[31] = ip2; frame[32] = ip3; frame[33] = ip4; /* Dest IP */

    uint16_t ip_cksum = calculate_ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ip_cksum >> 8);
    frame[25] = (uint8_t)(ip_cksum & 0xFF);

    /* 3. Layer 4: UDP Header */
    frame[34] = (uint8_t)(src_port >> 8); frame[35] = (uint8_t)(src_port & 0xFF);
    frame[36] = (uint8_t)(dst_port >> 8); frame[37] = (uint8_t)(dst_port & 0xFF);
    frame[38] = (uint8_t)(udp_len >> 8);  frame[39] = (uint8_t)(udp_len & 0xFF);

    memcpy(&frame[42], payload, payload_len);

    uint32_t tx_len = (total_len < ETH_MIN_FRAME_LEN) ? ETH_MIN_FRAME_LEN : total_len;

    char info[128];
    sprintf(info, "[TX UDP] Transmitting %u bytes (Src IP: %u.%u.%u.%u) to %u.%u.%u.%u:%u...\r\n",
            (unsigned int)tx_len, g_board_ip[0], g_board_ip[1], g_board_ip[2], g_board_ip[3],
            ip1, ip2, ip3, ip4, dst_port);
    PRINT_STRING(info);

    send_ethernet_frame(frame, tx_len);
}
static void send_tcp_packet(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4,
                            uint16_t src_port, uint16_t dst_port, uint8_t flags, const char *payload) {
    uint8_t frame[PACKET_MAX] = {0};
    uint32_t payload_len = (uint32_t)strlen(payload);
    uint32_t tcp_len = 20 + payload_len;
    uint32_t ip_len = 20 + tcp_len;
    uint32_t total_len = 14 + ip_len;

    /* 1. Layer 2: Broadcast Destination MAC so all local switch/Wi-Fi ports receive the frame */
    memset(&frame[0], 0xFF, 6);
    memcpy(&frame[6], g_test_mac->mac_addr, 6);
    frame[12] = 0x08; frame[13] = 0x00; /* IPv4 */

    /* 2. Layer 3: IPv4 Header using current assigned g_board_ip */
    frame[14] = 0x45; frame[15] = 0x00;
    frame[16] = (uint8_t)(ip_len >> 8); frame[17] = (uint8_t)(ip_len & 0xFF);
    frame[18] = 0xAB; frame[19] = 0xCD;
    frame[20] = 0x40; frame[21] = 0x00;
    frame[22] = 64;   frame[23] = 6;    /* TCP Protocol */
    memcpy(&frame[26], g_board_ip, 4);  /* Source IP (from DHCP) */
    frame[30] = ip1; frame[31] = ip2; frame[32] = ip3; frame[33] = ip4; /* Dest IP */

    uint16_t ip_cksum = calculate_ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ip_cksum >> 8);
    frame[25] = (uint8_t)(ip_cksum & 0xFF);

    /* 3. Layer 4: TCP Header */
    frame[34] = (uint8_t)(src_port >> 8); frame[35] = (uint8_t)(src_port & 0xFF);
    frame[36] = (uint8_t)(dst_port >> 8); frame[37] = (uint8_t)(dst_port & 0xFF);
    frame[38] = 0x00; frame[39] = 0x00; frame[40] = 0x00; frame[41] = 0x01; /* Sequence Number */
    frame[42] = 0x00; frame[43] = 0x00; frame[44] = 0x00; frame[45] = 0x00; /* Acknowledgment Number */
    frame[46] = 0x50;                             /* Data Offset: 20 Bytes */
    frame[47] = flags;                            /* TCP Flags (e.g., 0x02 for SYN, 0x18 for PSH+ACK) */
    frame[48] = 0xFA; frame[49] = 0xF0;           /* Window Size */

    if (payload_len > 0) {
        memcpy(&frame[54], payload, payload_len);
    }

    uint32_t tx_len = (total_len < ETH_MIN_FRAME_LEN) ? ETH_MIN_FRAME_LEN : total_len;

    char info[128];
    sprintf(info, "[TX TCP] Transmitting %u bytes (Flags: 0x%02X, Src IP: %u.%u.%u.%u) to %u.%u.%u.%u:%u...\r\n",
            (unsigned int)tx_len, flags, g_board_ip[0], g_board_ip[1], g_board_ip[2], g_board_ip[3],
            ip1, ip2, ip3, ip4, dst_port);
    PRINT_STRING(info);

    send_ethernet_frame(frame, tx_len);
}

static void test_dhcp_discover(void) {
    uint8_t frame[512] = {0};
    uint32_t xid = 0x3903F326;

    /* 1. Layer 2: Broadcast Ethernet Header */
    memset(&frame[0], 0xFF, 6);
    memcpy(&frame[6], g_test_mac->mac_addr, 6);
    frame[12] = 0x08; frame[13] = 0x00;

    uint16_t dhcp_payload_len = 256;
    uint16_t udp_len = 8 + dhcp_payload_len;
    uint16_t ip_len = 20 + udp_len;
    uint32_t total_len = 14 + ip_len;

    /* 2. Layer 3: IPv4 Header (0.0.0.0 -> 255.255.255.255) */
    frame[14] = 0x45; frame[15] = 0x00;
    frame[16] = (uint8_t)(ip_len >> 8); frame[17] = (uint8_t)(ip_len & 0xFF);
    frame[18] = 0x00; frame[19] = 0x01;
    frame[20] = 0x00; frame[21] = 0x00;
    frame[22] = 128;  frame[23] = 17; /* UDP */
    memset(&frame[26], 0x00, 4);
    memset(&frame[30], 0xFF, 4);

    uint16_t ip_cksum = calculate_ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ip_cksum >> 8);
    frame[25] = (uint8_t)(ip_cksum & 0xFF);

    /* 3. Layer 4: UDP Header (Client Port 68 -> Server Port 67) */
    frame[34] = 0x00; frame[35] = 68;
    frame[36] = 0x00; frame[37] = 67;
    frame[38] = (uint8_t)(udp_len >> 8); frame[39] = (uint8_t)(udp_len & 0xFF);

    /* 4. Layer 5: DHCP Header Structure */
    uint8_t *dhcp = &frame[42];
    dhcp[0] = 0x01; dhcp[1] = 0x01; dhcp[2] = 0x06; dhcp[3] = 0x00;
    dhcp[4] = (xid >> 24) & 0xFF; dhcp[5] = (xid >> 16) & 0xFF;
    dhcp[6] = (xid >> 8) & 0xFF;  dhcp[7] = xid & 0xFF;
    dhcp[10] = 0x80; /* Broadcast flag */
    memcpy(&dhcp[28], g_test_mac->mac_addr, 6);

    /* DHCP Magic Cookie (0x63825363) & Options */
    dhcp[236] = 0x63; dhcp[237] = 0x82; dhcp[238] = 0x53; dhcp[239] = 0x63;
    dhcp[240] = 53; dhcp[241] = 1; dhcp[242] = 1; /* Option 53: Discover */
    dhcp[243] = 55; dhcp[244] = 3; dhcp[245] = 1; dhcp[246] = 3; dhcp[247] = 6;
    dhcp[248] = 12; dhcp[249] = 5; memcpy(&dhcp[250], "Polar", 5);
    dhcp[255] = 255;

    /* Arm capture engine to catch the response */
    g_auto_rearm = false;
    g_capture = PACKET_ARMED;

    PRINT_STRING("[DHCP] Transmitting DHCP Discover Broadcast...\r\n");
    send_ethernet_frame(frame, total_len);

    PRINT_STRING("[DHCP] Listening for DHCP Offer on UDP Port 68...\r\n");

    /* 5. Listen Loop with Timeout */
    uint32_t timeout = 500000;
    while (timeout--) {
        if (PACKET_DONE == g_capture) {
            uint8_t *rx_buf = g_packet_data;
            uint32_t rx_len = g_packet_length;

            if (rx_len >= 282) {
                uint16_t ethertype = (rx_buf[12] << 8) | rx_buf[13];
                uint8_t protocol = rx_buf[23];
                uint16_t dst_port = (rx_buf[36] << 8) | rx_buf[37];

                /* Filter for IPv4 + UDP + Destination Port 68 */
                if (ethertype == 0x0800 && protocol == 17 && dst_port == 68) {
                    uint8_t *rx_dhcp = &rx_buf[42];
                    uint32_t rx_xid = (rx_dhcp[4] << 24) | (rx_dhcp[5] << 16) | (rx_dhcp[6] << 8) | rx_dhcp[7];

                    if (rx_xid == xid && rx_dhcp[0] == 0x02) { /* BOOTREPLY */
                        memcpy(g_board_ip, &rx_dhcp[16], 4);
                        char info[128];
                        PRINT_STRING("\r\n================ DHCP OFFER RECEIVED ================\r\n");
                        sprintf(info, "Offered IP Addr : %u.%u.%u.%u\r\n", g_board_ip[0], g_board_ip[1], g_board_ip[2], g_board_ip[3]); PRINT_STRING(info);
                        sprintf(info, "Server IP Addr  : %u.%u.%u.%u\r\n", rx_dhcp[20], rx_dhcp[21], rx_dhcp[22], rx_dhcp[23]); PRINT_STRING(info);
                        PRINT_STRING("=====================================================\r\n\r\n");
                        g_capture = PACKET_IDLE;
                        return;
                    }
                }
            }
            /* Re-arm if ambient traffic arrived instead of DHCP offer */
            g_capture = PACKET_ARMED;
        }
        for (volatile int delay = 0; delay < 100; delay++);
    }

    g_capture = PACKET_IDLE;
    PRINT_STRING("[DHCP] Timeout: No DHCP Offer received from local router.\r\n");
}

static void scan_mdio_bus(void) {
    char info[128];
    uint8_t found_cnt = 0;
    PRINT_STRING("\r\n--- Scanning MDIO Bus via GEM0 (Addresses 0-31) ---\r\n");

    for (uint8_t addr = 0; addr < 32; addr++) {
        uint16_t id1 = MSS_MAC_read_phy_reg(g_test_mac, addr, MII_PHYSID1);
        uint16_t id2 = MSS_MAC_read_phy_reg(g_test_mac, addr, MII_PHYSID2);

        if (id1 != 0xFFFFU && id1 != 0x0000U) {
            sprintf(info, "  [FOUND] Addr 0x%02X | ID: 0x%04X:0x%04X %s\r\n",
                    addr, id1, id2, (addr == g_phy_addr) ? "<-- Active VSC8221" : "");
            PRINT_STRING(info);
            found_cnt++;
        }
    }

    if (found_cnt == 0) {
        PRINT_STRING("  [ERROR] No PHY responded on MDIO bus.\r\n");
    } else {
        sprintf(info, "-------------------------------------------\r\nActive PHY: 0x%02X\r\n\r\n", g_phy_addr);
        PRINT_STRING(info);
    }
}

static void dump_gem_registers(void) {
    char info[128];
    PRINT_STRING("\r\n=== Hard MSS GEM0 MAC Registers ===\r\n");
    sprintf(info, "0x000 Net Control    : 0x%08X\r\n", (unsigned int)gem_read_reg(0x000)); PRINT_STRING(info);
    sprintf(info, "0x004 Net Config     : 0x%08X\r\n", (unsigned int)gem_read_reg(0x004)); PRINT_STRING(info);
    sprintf(info, "0x008 Net Status     : 0x%08X\r\n", (unsigned int)gem_read_reg(0x008)); PRINT_STRING(info);
    sprintf(info, "0x014 TX Status      : 0x%08X\r\n", (unsigned int)gem_read_reg(0x014)); PRINT_STRING(info);
    sprintf(info, "0x018 RX Q Pointer   : 0x%08X\r\n", (unsigned int)gem_read_reg(0x018)); PRINT_STRING(info);
    sprintf(info, "0x01C TX Q Pointer   : 0x%08X\r\n", (unsigned int)gem_read_reg(0x01C)); PRINT_STRING(info);
    sprintf(info, "0x020 RX Status      : 0x%08X\r\n", (unsigned int)gem_read_reg(0x020)); PRINT_STRING(info);
    sprintf(info, "0x080 Station Addr1B : 0x%08X\r\n", (unsigned int)gem_read_reg(0x080)); PRINT_STRING(info);
    sprintf(info, "0x084 Station Addr1T : 0x%08X\r\n", (unsigned int)gem_read_reg(0x084)); PRINT_STRING(info);
    PRINT_STRING("-----------------------------------\r\n\r\n");
}

static void dump_phy_registers(void) {
    char info[128];
    sprintf(info, "\r\n=== VSC8221 PHY Registers (Addr 0x%02X) ===\r\n", g_phy_addr);
    PRINT_STRING(info);
    for (uint8_t reg = 0; reg < 16; reg++) {
        uint16_t val = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, reg);
        sprintf(info, "Reg 0x%02X: 0x%04X\r\n", reg, val);
        PRINT_STRING(info);
    }
    PRINT_STRING("\r\n");
}

static bool get_cli_line(char *line_buf, size_t max_len) {
    static size_t idx = 0;
    uint8_t rx_byte = 0;

    if (MSS_UART_get_rx(DEMO_UART, &rx_byte, 1) > 0) {
        if (rx_byte == '\r' || rx_byte == '\n') {
            PRINT_STRING("\r\n");
            line_buf[idx] = '\0';
            idx = 0;
            return true;
        }
        else if (rx_byte == '\b' || rx_byte == 0x7F) {
            if (idx > 0) {
                idx--;
                PRINT_STRING("\b \b");
            }
        }
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
    uint32_t p1 = 0, p2 = 0;

    SYSREG->SOFT_RESET_CR = 0U;
    SYSREG->SUBBLK_CLOCK_CR = 0xFFFFFFFFUL;

    __disable_local_irq((int8_t)MMUART0_E51_INT);
    SysTick_Config();

    MSS_UART_init(DEMO_UART, MSS_UART_115200_BAUD, MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
    PRINT_STRING("\r\n***** PolarFire SoC Discovery Kit - Step 3 Test *****\r\n");
    __enable_irq();

    low_level_init();
    print_help();

    PRINT_STRING("eth-disco> ");

    while (1) {
        prvLinkStatusTask();

        if (PACKET_DONE == g_capture) {
            packet_dump();
            PRINT_STRING("eth-disco> ");
        }

        if (get_cli_line(cli_input, sizeof(cli_input))) {
            char *cmd = cli_input;
            while (*cmd == ' ') cmd++;

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
            else if (strcmp(cmd, "loopback phy") == 0 || strcmp(cmd, "phy loopback") == 0) {
                uint16_t bmcr = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_BMCR);
                bmcr ^= BMCR_LOOPBACK;
                MSS_MAC_write_phy_reg(g_test_mac, g_phy_addr, MII_BMCR, bmcr);

                if (bmcr & BMCR_LOOPBACK) {
                    PRINT_STRING("[CMD] VSC8221 PHY Near-End Loopback Enabled\r\n");
                } else {
                    PRINT_STRING("[CMD] VSC8221 PHY Loopback Disabled\r\n");
                }
            }

            else if (strcmp(cmd, "loopback off") == 0) {
                /* 1. Clear GEM PCS Loopback */
                g_test_mac->mac_base->PCS_CONTROL &= ~GEM_LOOPBACK_MODE;

                /* 2. Clear MAC Local Loopback */
                g_test_mac->mac_base->NETWORK_CONTROL &= ~(GEM_LOOPBACK_LOCAL | GEM_LOOPBACK);

                /* 3. Clear VSC8221 PHY Loopback */
                uint16_t bmcr = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_BMCR);
                MSS_MAC_write_phy_reg(g_test_mac, g_phy_addr, MII_BMCR, bmcr & ~BMCR_LOOPBACK);

                PRINT_STRING("[CMD] All Loopback Modes Disabled\r\n");
            }

            else if (strcmp(cmd, "capture") == 0 || strcmp(cmd, "capture single") == 0) {
                g_auto_rearm = false;
                g_capture = PACKET_ARMED;
                PRINT_STRING("[CMD] Single-Packet Capture Armed...\r\n");
            }
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
            else if (strcmp(cmd, "rx inspect") == 0 || strcmp(cmd, "inspect") == 0) {
                inspect_rx_packet();
            }
            else if (sscanf(cmd, "packet diff %u", &p1) == 1 || strcmp(cmd, "packet diff") == 0) {
                compare_tx_rx_buffers(p1);
            }
            else if (strcmp(cmd, "desc inspect") == 0 || strcmp(cmd, "dma status") == 0) {
                inspect_dma_descriptors();
            }
            else if (strcmp(cmd, "tx arp") == 0 || strcmp(cmd, "tx") == 0) {
                memcpy(&tx_pak_arp[6], g_test_mac->mac_addr, 6);
                int st = send_ethernet_frame(tx_pak_arp, sizeof(tx_pak_arp));
                sprintf(info, "[CMD] Transmitted ARP Packet. Driver Status: %d\r\n", st);
                PRINT_STRING(info);
            }
            else if (strncmp(cmd, "tx udp ", 7) == 0) {
                uint32_t ip1, ip2, ip3, ip4, src_port, dst_port;
                char payload[128] = {0};
                if (sscanf(cmd, "tx udp %u.%u.%u.%u %u %u %s", &ip1, &ip2, &ip3, &ip4, &src_port, &dst_port, payload) >= 6) {
                    send_udp_packet((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4,
                                    (uint16_t)src_port, (uint16_t)dst_port, payload[0] ? payload : "PolarFire_Test");
                } else {
                    PRINT_STRING("Usage: tx udp <ip_addr> <src_port> <dst_port> [payload_string]\r\n");
                }
            }
            else if (strncmp(cmd, "tx tcp ", 7) == 0) {
                uint32_t ip1, ip2, ip3, ip4, src_port, dst_port, flags;
                char payload[128] = {0};
                if (sscanf(cmd, "tx tcp %u.%u.%u.%u %u %u %x %s", &ip1, &ip2, &ip3, &ip4, &src_port, &dst_port, &flags, payload) >= 7) {
                    send_tcp_packet((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4,
                                    (uint16_t)src_port, (uint16_t)dst_port, (uint8_t)flags, payload);
                } else {
                    PRINT_STRING("Usage: tx tcp <ip_addr> <src_port> <dst_port> <flags_hex> [payload_string]\r\n");
                }
            }
            else if (strcmp(cmd, "dhcp") == 0) {
                test_dhcp_discover();
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
            else if (strcmp(cmd, "mdio scan") == 0) {
                scan_mdio_bus();
            }
            else if (sscanf(cmd, "mdio read %x", &p1) == 1) {
                uint16_t val = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, (uint8_t)p1);
                sprintf(info, "MDIO Reg[0x%02X] @ Addr 0x%02X = 0x%04X\r\n", (unsigned int)p1, g_phy_addr, val);
                PRINT_STRING(info);
            }
            else if (sscanf(cmd, "mdio write %x %x", &p1, &p2) == 2) {
                MSS_MAC_write_phy_reg(g_test_mac, g_phy_addr, (uint8_t)p1, (uint16_t)p2);
                sprintf(info, "MDIO Reg[0x%02X] @ Addr 0x%02X <= 0x%04X\r\n", (unsigned int)p1, g_phy_addr, (unsigned int)p2);
                PRINT_STRING(info);
            }
            else if (strcmp(cmd, "phy dump") == 0) {
                dump_phy_registers();
            }
            else if (strcmp(cmd, "gem dump") == 0) {
                dump_gem_registers();
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
