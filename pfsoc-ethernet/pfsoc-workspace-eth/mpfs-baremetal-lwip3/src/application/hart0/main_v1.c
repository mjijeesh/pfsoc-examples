/*******************************************************************************
 * PolarFire SoC Discovery Kit - High-Stability lwIP Network Engine
 * Target: MPFS GEM0 + VSC8221 SGMII PHY (MDIO Address 11)
 * Static IP: 192.168.2.207
 * Includes RAW ICMP Ping Client Implementation
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

/* lwIP Stack Headers */
#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"

/* Required headers for ICMP & RAW APIs */
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"

#ifndef TARGET_DISCOVERY_KIT
#define TARGET_DISCOVERY_KIT
#endif

#ifndef TARGET_G5_SOC
#define TARGET_G5_SOC
#endif

#define DEMO_UART       &g_mss_uart0_lo
#define PRINT_STRING(x) MSS_UART_polled_tx_string(DEMO_UART, (const uint8_t *)x);

#define PACKET_MAX      1518U
#define TX_SLOT_SIZE    1536U  /* 16-byte aligned */
#define ETH_MIN_LEN     60U
#define TX_RING_SLOTS   8U
#define RX_QUEUE_SIZE   16U

/* Raw Frame Container */
typedef struct {
    uint8_t data[PACKET_MAX];
    uint32_t len;
} raw_frame_t;

static raw_frame_t g_rx_raw_queue[RX_QUEUE_SIZE];
static volatile uint32_t g_rx_q_head = 0;
static volatile uint32_t g_rx_q_tail = 0;

/* Hardware Aligned Buffers */
static uint8_t g_mac_rx_buffer[MSS_MAC_RX_RING_SIZE][MSS_MAC_MAX_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t g_mac_tx_buffer[TX_RING_SLOTS][TX_SLOT_SIZE] __attribute__((aligned(16)));
static volatile uint32_t g_tx_ring_head = 0;

static mss_mac_cfg_t g_mac_config;
mss_mac_instance_t *g_test_mac = &g_mac0;

static volatile uint64_t g_tx_count = 0;
static volatile uint64_t g_rx_count = 0;
static volatile uint64_t g_tick_counter = 0;

static uint8_t g_phy_addr = PHY_VSC8221_MDIO_ADDR; /* 11U */
static uint8_t g_board_ip[4] = {192, 168, 2, 207};

struct netif g_netif;

sys_prot_t sys_arch_protect(void) {
    __disable_irq();
    return 1;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void)pval;
    __enable_irq();
}

u32_t sys_now(void) {
    return (u32_t)g_tick_counter;
}

void E51_sysTick_IRQHandler(void) {
    g_tick_counter += HART0_TICK_RATE_MS;
}

/* Transmit Driver Interface */
static int send_ethernet_frame(const uint8_t *buf, uint32_t len) {
    if (len > PACKET_MAX) len = PACKET_MAX;

    if (g_test_mac->queue[0].nb_available_tx_desc == 0) {
        return MSS_MAC_FAILED;
    }

    uint32_t idx = g_tx_ring_head;
    g_tx_ring_head = (g_tx_ring_head + 1) % TX_RING_SLOTS;
    uint8_t *tx_buf = g_mac_tx_buffer[idx];

    memcpy(tx_buf, buf, len);

    if (len < ETH_MIN_LEN) {
        memset(&tx_buf[len], 0, ETH_MIN_LEN - len);
        len = ETH_MIN_LEN;
    }

    int32_t st = MSS_MAC_send_pkt(g_test_mac, 0, tx_buf, len, (void *)1);
    return (int)st;
}

/* lwIP Link Output Bridge */
static err_t lwip_mac_output(struct netif *netif, struct pbuf *p) {
    (void)netif;
    static uint8_t temp_buf[TX_SLOT_SIZE] __attribute__((aligned(16)));
    u16_t len = pbuf_copy_partial(p, temp_buf, p->tot_len, 0);

    int32_t st = send_ethernet_frame(temp_buf, len);
    return (st == MSS_MAC_SUCCESS) ? ERR_OK : ERR_MEM;
}

static err_t lwip_netif_init(struct netif *netif) {
    netif->name[0] = 'e';
    netif->name[1] = '0';
    netif->output = etharp_output;
    netif->linkoutput = lwip_mac_output;
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, g_test_mac->mac_addr, 6);
    return ERR_OK;
}

/* Fast Hardware ISR */
static void mac_rx_callback(void *this_mac, uint32_t queue_no, uint8_t *p_rx_packet, uint32_t pckt_length, mss_mac_rx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)queue_no;

    if (pckt_length > 0 && pckt_length <= PACKET_MAX) {
        g_rx_count++;

        uint32_t next = (g_rx_q_head + 1) % RX_QUEUE_SIZE;
        if (next != g_rx_q_tail) {
            memcpy(g_rx_raw_queue[g_rx_q_head].data, p_rx_packet, pckt_length);
            g_rx_raw_queue[g_rx_q_head].len = pckt_length;
            __sync_synchronize();
            g_rx_q_head = next;
        }
    }

    MSS_MAC_receive_pkt((mss_mac_instance_t *)this_mac, 0, p_rx_packet, 0, 1);
}

static void packet_tx_complete_handler(void *this_mac, uint32_t queue_no, mss_mac_tx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)this_mac; (void)queue_no;
    g_tx_count++;
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

static void lwip_network_init(void) {
    ip4_addr_t ipaddr, netmask, gw;

    IP4_ADDR(&ipaddr, g_board_ip[0], g_board_ip[1], g_board_ip[2], g_board_ip[3]);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, g_board_ip[0], g_board_ip[1], g_board_ip[2], 1);

    lwip_init();

    netif_add(&g_netif, &ipaddr, &netmask, &gw,
              g_test_mac,
              lwip_netif_init,
              ethernet_input);

    netif_set_default(&g_netif);
    netif_set_up(&g_netif);
    netif_set_link_up(&g_netif);

    PRINT_STRING("[lwIP] Isolated Network Engine Initialized.\r\n");
}

static void send_lwip_arp_request(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4) {
    ip4_addr_t target_ip;
    IP4_ADDR(&target_ip, ip1, ip2, ip3, ip4);

    char dbg[128];
    sprintf(dbg, "[ARP TX] Sent ARP Request for %u.%u.%u.%u\r\n", ip1, ip2, ip3, ip4);
    PRINT_STRING(dbg);

    etharp_request(&g_netif, &target_ip);
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

/* ICMP Receive Callback */
static u8_t ping_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    (void)arg; (void)pcb;
    if (p->tot_len >= (20 + sizeof(struct icmp_echo_hdr))) {
        struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)((u8_t *)p->payload + 20);
        if (ICMPH_TYPE(iecho) == ICMP_ER) {
            char buf[64];
            sprintf(buf, "[PING] Reply received from %s\r\n", ipaddr_ntoa(addr));
            PRINT_STRING(buf);
            pbuf_free(p);
            return 1;
        }
    }
    return 0;
}

/* Outbound ICMP Ping Generator */
static void send_ping(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4) {
    static struct raw_pcb *ping_pcb = NULL;
    static u16_t ping_seq = 0;
    ip_addr_t target;
    IP4_ADDR(&target, ip1, ip2, ip3, ip4);

    if (!ping_pcb) {
        ping_pcb = raw_new(IP_PROTO_ICMP);
        raw_recv(ping_pcb, ping_recv_cb, NULL);
        raw_bind(ping_pcb, IP_ADDR_ANY);
    }

    struct pbuf *p = pbuf_alloc(PBUF_RAW, sizeof(struct icmp_echo_hdr) + 32, PBUF_RAM);
    if (!p) return;

    struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)p->payload;
    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id = 0xAFAF;
    iecho->seqno = lwip_htons(++ping_seq);
    memset((char *)iecho + sizeof(struct icmp_echo_hdr), 'a', 32);
    iecho->chksum = inet_chksum(iecho, p->len);

    raw_sendto(ping_pcb, p, &target);
    pbuf_free(p);
}

void mac_task(void *pvParameters) {
    (void)pvParameters;
    char cli_input[128];
    char info[128];

    SYSREG->SOFT_RESET_CR = 0U;
    SYSREG->SUBBLK_CLOCK_CR = 0xFFFFFFFFUL;

    __disable_local_irq((int8_t)MMUART0_E51_INT);
    SysTick_Config();

    MSS_UART_init(DEMO_UART, MSS_UART_115200_BAUD, MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
    PRINT_STRING("\r\n=======================================================\r\n");
    PRINT_STRING(" PolarFire SoC - High-Stability lwIP Network Engine\r\n");
    PRINT_STRING("=======================================================\r\n");
    __enable_irq();

    low_level_init();
    lwip_network_init();

    PLIC_EnableIRQ(MAC0_INT_U54_INT);

    PRINT_STRING("eth-cli> ");

    while (1) {
        sys_check_timeouts();

        /* Safely Allocate pbuf & Feed lwIP in Main Thread Context */
        while (g_rx_q_head != g_rx_q_tail) {
            uint32_t current_tail = g_rx_q_tail;
            uint32_t len = g_rx_raw_queue[current_tail].len;
            uint8_t *data = g_rx_raw_queue[current_tail].data;

            struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
            if (p != NULL) {
                pbuf_take(p, data, (u16_t)len);
                g_netif.input(p, &g_netif);
            }

            __sync_synchronize();
            g_rx_q_tail = (current_tail + 1) % RX_QUEUE_SIZE;
        }

        /* Service CLI */
        if (get_cli_line(cli_input, sizeof(cli_input))) {
            char *cmd = cli_input;
            while (*cmd == ' ') cmd++;

            if (strncmp(cmd, "arp ", 4) == 0) {
                uint32_t ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
                if (sscanf(cmd + 4, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4) {
                    send_lwip_arp_request((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4);
                } else {
                    PRINT_STRING("Usage: arp <ip1>.<ip2>.<ip3>.<ip4>\r\n");
                }
            } else if (strcmp(cmd, "status") == 0) {
                sprintf(info, "[STATUS] IP: %s | TX Frames: %lu | RX Frames: %lu\r\n",
                        ipaddr_ntoa(&g_netif.ip_addr),
                        (unsigned long)g_tx_count, (unsigned long)g_rx_count);
                PRINT_STRING(info);
            } else if (strncmp(cmd, "ping ", 5) == 0) {
                uint32_t ip1, ip2, ip3, ip4;
                if (sscanf(cmd + 5, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4) {
                    send_ping((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4);
                } else {
                    PRINT_STRING("Usage: ping <ip1>.<ip2>.<ip3>.<ip4>\r\n");
                }
            } else if (strlen(cmd) > 0) {
                PRINT_STRING("Unknown command. Example: 'ping 192.168.2.197' or 'status'.\r\n");
            }

            PRINT_STRING("eth-cli> ");
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
