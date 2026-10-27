/*******************************************************************************
 * PolarFire SoC Discovery Kit - FreeRTOS + lwIP Multi-Threaded Application
 * GEM0 Ethernet MAC + VSC8221 SGMII PHY (MDIO Address 11)
 * Features: Static/DHCP IP | ICMP Ping | HTTPD | iPerf | Traffic Monitor | CLI
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

/* FreeRTOS Kernel Headers */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* lwIP Stack & OS Headers */
#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"

/* Protocol & Application Headers */
#include "lwip/dhcp.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/apps/lwiperf.h"

#define DEMO_UART       &g_mss_uart0_lo
#define PRINT_STRING(x) MSS_UART_polled_tx_string(DEMO_UART, (const uint8_t *)x);

#define PACKET_MAX      1518U
#define TX_SLOT_SIZE    1536U  /* 16-byte aligned */
#define ETH_MIN_LEN     60U
#define TX_RING_SLOTS   8U
#define RX_QUEUE_SIZE   16U

typedef struct {
    uint8_t data[PACKET_MAX];
    uint32_t len;
} raw_frame_t;

static raw_frame_t g_rx_raw_queue[RX_QUEUE_SIZE];
static volatile uint32_t g_rx_q_head = 0;
static volatile uint32_t g_rx_q_tail = 0;

static uint8_t g_mac_rx_buffer[MSS_MAC_RX_RING_SIZE][MSS_MAC_MAX_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t g_mac_tx_buffer[TX_RING_SLOTS][TX_SLOT_SIZE] __attribute__((aligned(16)));
static volatile uint32_t g_tx_ring_head = 0;

static mss_mac_cfg_t g_mac_config;
mss_mac_instance_t *g_test_mac = &g_mac0;

static volatile uint64_t g_tx_count = 0;
static volatile uint64_t g_rx_count = 0;

static uint8_t g_phy_addr = PHY_VSC8221_MDIO_ADDR;
static uint8_t g_board_ip[4] = {192, 168, 20, 207};

static SemaphoreHandle_t g_rx_sem = NULL;
static volatile bool g_debug_monitor = false;
static bool g_dhcp_requested = false;
static bool g_dhcp_bound_reported = false;
static bool g_last_link_state = true;

struct netif g_netif;

/* Embedded Web Server Virtual File System */
static const char g_html_page[] =
    "<html><head><title>PolarFire SoC FreeRTOS</title></head>"
    "<body><h1>PolarFire SoC FreeRTOS lwIP Node</h1>"
    "<p>Status: GEM0 MAC running under FreeRTOS multithreaded kernel.</p></body></html>";

err_t fs_open(struct fs_file *file, const char *name) {
    (void)name;
    memset(file, 0, sizeof(struct fs_file));
    file->data = g_html_page;
    file->len = sizeof(g_html_page) - 1;
    file->index = file->len;
    return ERR_OK;
}

void fs_close(struct fs_file *file) { (void)file; }
int fs_read(struct fs_file *file, char *buffer, int count) { (void)file; (void)buffer; (void)count; return FS_READ_EOF; }
int fs_bytes_left(struct fs_file *file) { (void)file; return 0; }

static void print_status(void);
static void print_help(void);

/* Transmit Driver */
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

    return (int)MSS_MAC_send_pkt(g_test_mac, 0, tx_buf, len, (void *)1);
}

static err_t lwip_mac_output(struct netif *netif, struct pbuf *p) {
    (void)netif;
    static uint8_t temp_buf[TX_SLOT_SIZE] __attribute__((aligned(16)));
    u16_t len = pbuf_copy_partial(p, temp_buf, p->tot_len, 0);

    return (send_ethernet_frame(temp_buf, len) == MSS_MAC_SUCCESS) ? ERR_OK : ERR_MEM;
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

/* Hardware Ethernet RX ISR */
static void mac_rx_callback(void *this_mac, uint32_t queue_no, uint8_t *p_rx_packet, uint32_t pckt_length, mss_mac_rx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)queue_no;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (pckt_length > 0 && pckt_length <= PACKET_MAX) {
        g_rx_count++;
        uint32_t next = (g_rx_q_head + 1) % RX_QUEUE_SIZE;
        if (next != g_rx_q_tail) {
            memcpy(g_rx_raw_queue[g_rx_q_head].data, p_rx_packet, pckt_length);
            g_rx_raw_queue[g_rx_q_head].len = pckt_length;
            __sync_synchronize();
            g_rx_q_head = next;

            if (g_rx_sem != NULL) {
                xSemaphoreGiveFromISR(g_rx_sem, &xHigherPriorityTaskWoken);
            }
        }
    }

    MSS_MAC_receive_pkt((mss_mac_instance_t *)this_mac, 0, p_rx_packet, 0, 1);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void packet_tx_complete_handler(void *this_mac, uint32_t queue_no, mss_mac_tx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)this_mac; (void)queue_no;
    g_tx_count++;
}

static void low_level_init(void) {
    PRINT_STRING("[DRIVER] Initializing MSS MAC configuration structures...\r\n");
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

    PRINT_STRING("[DRIVER] Executing MSS_MAC_init() and PHY Auto-negotiation...\r\n");
    MSS_MAC_init(g_test_mac, &g_mac_config);
    PRINT_STRING("[DRIVER] GEM0 MAC and VSC8221 SGMII PHY initialized successfully.\r\n");

    PRINT_STRING("[DRIVER] Registering TX and RX interrupt callbacks...\r\n");
    MSS_MAC_set_tx_callback(g_test_mac, 0, packet_tx_complete_handler);
    MSS_MAC_set_rx_callback(g_test_mac, 0, mac_rx_callback);

    PRINT_STRING("[DRIVER] Populating DMA receive ring buffers...\r\n");
    for (uint32_t count = 0; count < MSS_MAC_RX_RING_SIZE; ++count) {
        if (count != (MSS_MAC_RX_RING_SIZE - 1)) {
            MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, 0);
        } else {
            MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, -1);
        }
    }
    PRINT_STRING("[DRIVER] Low-level GEM0 DMA hardware setup complete.\r\n");
}

/* iPerf Completion Report Callback */
static void my_iperf_report_cb(void *arg, enum lwiperf_report_type report_type,
                                const ip_addr_t *local_addr, u16_t local_port,
                                const ip_addr_t *remote_addr, u16_t remote_port,
                                u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec) {
    (void)arg; (void)report_type; (void)local_addr; (void)local_port; (void)remote_addr; (void)remote_port;
    char buf[160];
    sprintf(buf, "\r\n[IPERF REPORT] Transferred: %lu KB | Duration: %lu ms | Bandwidth: %lu.%02lu Mbps\r\neth-cli> ",
            (unsigned long)(bytes_transferred / 1024),
            (unsigned long)ms_duration,
            (unsigned long)(bandwidth_kbitpsec / 1000),
            (unsigned long)((bandwidth_kbitpsec % 1000) / 10));
    PRINT_STRING(buf);
}

/* TCPIP Thread Completion Callback */
static void tcpip_init_done_cb(void *arg) {
    (void)arg;
    ip4_addr_t ipaddr, netmask, gw;

    PRINT_STRING("[STACK] lwIP tcpip_thread initialized. Setting up netif...\r\n");

    IP4_ADDR(&ipaddr, g_board_ip[0], g_board_ip[1], g_board_ip[2], g_board_ip[3]);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, g_board_ip[0], g_board_ip[1], g_board_ip[2], 1);

    netif_add(&g_netif, &ipaddr, &netmask, &gw,
              g_test_mac,
              lwip_netif_init,
              tcpip_input);

    netif_set_default(&g_netif);
    netif_set_up(&g_netif);
    netif_set_link_up(&g_netif);
    PRINT_STRING("[STACK] Interface e0 added, configured, and set to LINK_UP.\r\n");

    PRINT_STRING("[SERVICES] Starting HTTP Server (Port 80)...\r\n");
    httpd_init();

    PRINT_STRING("[SERVICES] Starting iPerf Bandwidth Server (Port 5001)...\r\n");
    lwiperf_start_tcp_server_default(my_iperf_report_cb, NULL);

    PRINT_STRING("[DEBUG] tcpip_thread running. All network services active.\r\n");
    print_status();
}

/* Task 1: Ethernet RX Deferred Task with Real-Time Traffic Inspector */
static void eth_rx_task(void *pvParameters) {
    (void)pvParameters;
    char dbg_buf[128];

    while (1) {
        if (xSemaphoreTake(g_rx_sem, portMAX_DELAY) == pdTRUE) {
            while (g_rx_q_head != g_rx_q_tail) {
                uint32_t current_tail = g_rx_q_tail;
                uint32_t len = g_rx_raw_queue[current_tail].len;
                uint8_t *data = g_rx_raw_queue[current_tail].data;

                /* Real-Time Traffic Inspector */
                if (g_debug_monitor && len >= 14) {
                    uint16_t ethertype = (data[12] << 8) | data[13];
                    if (ethertype == 0x0806 && len >= 28) { /* ARP */
                        uint16_t op = (data[20] << 8) | data[21];
                        if (op == 1 && data[38] == g_board_ip[0] && data[39] == g_board_ip[1]) {
                            sprintf(dbg_buf, "\r\n[DEBUG] RX ARP Request for %u.%u.%u.%u from %u.%u.%u.%u\r\neth-cli> ",
                                    data[38], data[39], data[40], data[41],
                                    data[28], data[29], data[30], data[31]);
                            PRINT_STRING(dbg_buf);
                        }
                    } else if (ethertype == 0x0800 && len >= 34) { /* IPv4 */
                        uint8_t proto = data[23];
                        if (proto == 1 && len >= 38 && data[34] == 8) { /* ICMP Echo Req */
                            sprintf(dbg_buf, "\r\n[DEBUG] RX Inbound Ping Request from %u.%u.%u.%u\r\neth-cli> ",
                                    data[26], data[27], data[28], data[29]);
                            PRINT_STRING(dbg_buf);
                        } else if (proto == 6 && len >= 36) { /* TCP */
                            uint16_t dport = (data[36] << 8) | data[37];
                            if (dport == 80 && (data[47] & 0x02)) {
                                sprintf(dbg_buf, "\r\n[DEBUG] HTTP Request (Port 80) from %u.%u.%u.%u\r\neth-cli> ",
                                        data[26], data[27], data[28], data[29]);
                                PRINT_STRING(dbg_buf);
                            } else if (dport == 5001 && (data[47] & 0x02)) {
                                sprintf(dbg_buf, "\r\n[DEBUG] iPerf Client Connection (Port 5001) from %u.%u.%u.%u\r\neth-cli> ",
                                        data[26], data[27], data[28], data[29]);
                                PRINT_STRING(dbg_buf);
                            }
                        }
                    }
                }

                struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
                if (p != NULL) {
                    pbuf_take(p, data, (u16_t)len);

                    if (g_netif.input(p, &g_netif) != ERR_OK) {
                        pbuf_free(p);
                    }
                }

                __sync_synchronize();
                g_rx_q_tail = (current_tail + 1) % RX_QUEUE_SIZE;
            }
        }
    }
}

/* Task 2: PHY Link Monitoring Task */
static void phy_monitor_task(void *pvParameters) {
    (void)pvParameters;
    char dbg_buf[128];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        mss_mac_speed_t speed;
        uint8_t fullduplex;
        uint8_t link_up = MSS_MAC_get_link_status(g_test_mac, &speed, &fullduplex);

        if (link_up && !g_last_link_state) {
            g_last_link_state = true;
            netif_set_link_up(&g_netif);
            PRINT_STRING("\r\n[PHY EVENT] Link RESTORED.\r\n");

            if (g_dhcp_requested) {
                g_dhcp_bound_reported = false;
                dhcp_stop(&g_netif);
                netif_set_addr(&g_netif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4);
                dhcp_start(&g_netif);
            }
            PRINT_STRING("eth-cli> ");
        } else if (!link_up && g_last_link_state) {
            g_last_link_state = false;
            netif_set_link_down(&g_netif);
            PRINT_STRING("\r\n[PHY WARNING] Link LOST.\r\neth-cli> ");
        }

        if (g_dhcp_requested && dhcp_supplied_address(&g_netif) && !g_dhcp_bound_reported) {
            g_dhcp_bound_reported = true;
            sprintf(dbg_buf, "\r\n[DHCP SUCCESS] IP: %s\r\neth-cli> ", ipaddr_ntoa(&g_netif.ip_addr));
            PRINT_STRING(dbg_buf);
        }
    }
}

/* Outbound Ping Implementation */
static u8_t ping_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    (void)arg; (void)pcb;
    if (p->tot_len >= (20 + sizeof(struct icmp_echo_hdr))) {
        struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)((u8_t *)p->payload + 20);
        if (ICMPH_TYPE(iecho) == ICMP_ER) {
            char buf[128];
            sprintf(buf, "[DEBUG] ICMP Echo Reply received from %s (Seq: %u)\r\neth-cli> ",
                    ipaddr_ntoa(addr), lwip_ntohs(iecho->seqno));
            PRINT_STRING(buf);
            pbuf_free(p);
            return 1;
        }
    }
    return 0;
}

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

    char dbg[128];
    sprintf(dbg, "[DEBUG] Transmitting ICMP Echo Request to %u.%u.%u.%u (Seq: %u)\r\n", ip1, ip2, ip3, ip4, ping_seq);
    PRINT_STRING(dbg);

    raw_sendto(ping_pcb, p, &target);
    pbuf_free(p);
}

/* Manual ARP Request Generator */
static void send_lwip_arp_request(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4) {
    ip4_addr_t target_ip;
    IP4_ADDR(&target_ip, ip1, ip2, ip3, ip4);

    char dbg[128];
    sprintf(dbg, "[DEBUG] Transmitting Manual ARP Request for %u.%u.%u.%u\r\n", ip1, ip2, ip3, ip4);
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
        } else if (rx_byte == '\b' || rx_byte == 0x7F) {
            if (idx > 0) {
                idx--;
                PRINT_STRING("\b \b");
            }
        } else if (idx < max_len - 1 && rx_byte >= 32 && rx_byte <= 126) {
            uint8_t echo[2] = {rx_byte, 0};
            PRINT_STRING(echo);
            line_buf[idx++] = (char)rx_byte;
        }
    }
    return false;
}

static void print_help(void) {
    PRINT_STRING("\r\n======================== Available CLI Commands ========================\r\n");
    PRINT_STRING("  help                   - Display this command documentation menu\r\n");
    PRINT_STRING("  status                 - Show Network Interface details, MAC, IP & Packets\r\n");
    PRINT_STRING("  monitor <on|off>       - Enable or disable background packet debug output\r\n");
    PRINT_STRING("  dhcp                   - Initiate DHCP DISCOVER to request dynamic IP\r\n");
    PRINT_STRING("  ping <ip1.ip2.ip3.ip4> - Send outbound ICMP Echo Request to target host\r\n");
    PRINT_STRING("  arp <ip1.ip2.ip3.ip4>  - Send manual Layer 2 ARP Request for target IP\r\n");
    PRINT_STRING("------------------------------------------------------------------------\r\n");
    PRINT_STRING("  [Active Background Services]\r\n");
    PRINT_STRING("  HTTP Web Server        - Port 80   (Open http://<board_ip> in browser)\r\n");
    PRINT_STRING("  iPerf Bandwidth Server - Port 5001 (Run iperf test from host PC)\r\n");
    PRINT_STRING("========================================================================\r\n");
}

static void print_status(void) {
    char info[400];
    char ip_str[16], nm_str[16], gw_str[16];

    ipaddr_ntoa_r(&g_netif.ip_addr, ip_str, sizeof(ip_str));
    ipaddr_ntoa_r(&g_netif.netmask, nm_str, sizeof(nm_str));
    ipaddr_ntoa_r(&g_netif.gw, gw_str, sizeof(gw_str));

    sprintf(info, "\r\n[NETWORK STATUS]\r\n"
                  "  Execution    : FreeRTOS Task Kernel Active\r\n"
                  "  Interface    : %c%c\r\n"
                  "  MAC          : %02X:%02X:%02X:%02X:%02X:%02X\r\n"
                  "  Link         : %s\r\n"
                  "  IPv4 Address : %s\r\n"
                  "  Subnet Mask  : %s\r\n"
                  "  Gateway      : %s\r\n"
                  "  DHCP State   : %s\r\n"
                  "  Traffic Mon  : %s\r\n"
                  "  Tx Frames    : %lu\r\n"
                  "  Rx Frames    : %lu\r\n",
                  g_netif.name[0], g_netif.name[1],
                  g_netif.hwaddr[0], g_netif.hwaddr[1], g_netif.hwaddr[2],
                  g_netif.hwaddr[3], g_netif.hwaddr[4], g_netif.hwaddr[5],
                  g_last_link_state ? "UP" : "DOWN",
                  ip_str, nm_str, gw_str,
                  dhcp_supplied_address(&g_netif) ? "BOUND" : (g_dhcp_requested ? "DISCOVERING..." : "DISABLED"),
                  g_debug_monitor ? "ENABLED (ON)" : "DISABLED (OFF)",
                  (unsigned long)g_tx_count, (unsigned long)g_rx_count);
    PRINT_STRING(info);
}

/* Task 3: Interactive CLI Task */
static void cli_task(void *pvParameters) {
    (void)pvParameters;
    char cli_input[128];

    PRINT_STRING("eth-cli> ");

    while (1) {
        if (get_cli_line(cli_input, sizeof(cli_input))) {
            char *cmd = cli_input;
            while (*cmd == ' ') cmd++;

            if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
                print_help();
            } else if (strcmp(cmd, "status") == 0) {
                print_status();
            } else if (strcmp(cmd, "monitor on") == 0) {
                g_debug_monitor = true;
                PRINT_STRING("[DEBUG] Real-time traffic monitoring ENABLED.\r\n");
            } else if (strcmp(cmd, "monitor off") == 0) {
                g_debug_monitor = false;
                PRINT_STRING("[DEBUG] Real-time traffic monitoring DISABLED.\r\n");
            } else if (strcmp(cmd, "dhcp") == 0) {
                PRINT_STRING("[DEBUG] Requesting DHCP...\r\n");
                g_dhcp_requested = true;
                g_dhcp_bound_reported = false;
                dhcp_stop(&g_netif);
                netif_set_addr(&g_netif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4);
                dhcp_start(&g_netif);
            } else if (strncmp(cmd, "arp ", 4) == 0) {
                uint32_t ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
                if (sscanf(cmd + 4, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4) {
                    send_lwip_arp_request((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4);
                } else {
                    PRINT_STRING("Usage: arp <ip1>.<ip2>.<ip3>.<ip4>\r\n");
                }
            } else if (strncmp(cmd, "ping ", 5) == 0) {
                uint32_t ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
                if (sscanf(cmd + 5, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4) {
                    send_ping((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4);
                } else {
                    PRINT_STRING("Usage: ping <ip1>.<ip2>.<ip3>.<ip4>\r\n");
                }
            } else if (strlen(cmd) > 0) {
                PRINT_STRING("Unknown command. Type 'help' for available CLI commands.\r\n");
            }

            PRINT_STRING("eth-cli> ");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void e51(void) {
    write_csr(mscratch, 0);
    write_csr(mcause, 0);
    write_csr(mepc, 0);
    PLIC_init();

    SYSREG->SOFT_RESET_CR = 0U;
    SYSREG->SUBBLK_CLOCK_CR = 0xFFFFFFFFUL;

    __disable_local_irq((int8_t)MMUART0_E51_INT);

    MSS_UART_init(DEMO_UART, MSS_UART_115200_BAUD, MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
    PRINT_STRING("\r\n=======================================================\r\n");
    PRINT_STRING(" PolarFire SoC - FreeRTOS + lwIP Network Engine\r\n");
    PRINT_STRING("=======================================================\r\n");

    PRINT_STRING("[INIT] Configured MMUART0 at 115200 Baud.\r\n");

    extern void freertos_risc_v_trap_handler(void);
    write_csr(mtvec, (uint64_t)freertos_risc_v_trap_handler);
    PRINT_STRING("[INIT] Machine Trap Vector (mtvec) assigned to FreeRTOS Handler.\r\n");

    PRINT_STRING("[INIT] Initializing Low-Level GEM0 MAC & VSC8221 PHY...\r\n");
    low_level_init();

    PRINT_STRING("[INIT] Creating Ethernet RX Semaphore...\r\n");
    g_rx_sem = xSemaphoreCreateBinary();

    PRINT_STRING("[INIT] Launching lwIP Core (tcpip_init)...\r\n");
    tcpip_init(tcpip_init_done_cb, NULL);

    PRINT_STRING("[INIT] Creating FreeRTOS Tasks...\r\n");
    xTaskCreate(eth_rx_task, "ETH_RX", 1024, NULL, configMAX_PRIORITIES - 1, NULL);
    PRINT_STRING("  -> Created Task: ETH_RX (Priority: High)\r\n");

    xTaskCreate(phy_monitor_task, "PHY_MON", 512, NULL, tskIDLE_PRIORITY + 2, NULL);
    PRINT_STRING("  -> Created Task: PHY_MON (Priority: Medium)\r\n");

    xTaskCreate(cli_task, "CLI_TASK", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
    PRINT_STRING("  -> Created Task: CLI_TASK (Priority: Low)\r\n");

    PRINT_STRING("[INIT] Enabling GEM0 Ethernet Interrupts in PLIC...\r\n");
    PLIC_SetPriority(MAC0_INT_U54_INT, 2);
    PLIC_EnableIRQ(MAC0_INT_U54_INT);

    __enable_irq();
    PRINT_STRING("[INIT] Global interrupts enabled.\r\n");

    PRINT_STRING("[INIT] Starting FreeRTOS Kernel Scheduler...\r\n");
    vTaskStartScheduler();

    while (1);
}

int main(void) {
    e51();
    return 0;
}

/* FreeRTOS Required Application Hook Functions */

void vAssertCalled(void) {
    taskDISABLE_INTERRUPTS();
    PRINT_STRING("\r\n[FREERTOS ASSERT FAILED!]\r\n");
    for (;;);
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    PRINT_STRING("\r\n[FREERTOS ERROR] Dynamic Malloc Failed (Heap Exhausted)!\r\n");
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    taskDISABLE_INTERRUPTS();
    PRINT_STRING("\r\n[FREERTOS ERROR] Stack Overflow Detected in Task: ");
    if (pcTaskName != NULL) {
        PRINT_STRING(pcTaskName);
    }
    PRINT_STRING("\r\n");
    for (;;);
}

extern void handle_m_ext_interrupt(void);

void freertos_risc_v_application_interrupt_handler(uint32_t mcause) {
    (void)mcause;
    handle_m_ext_interrupt();
}
