/*******************************************************************************
 * PolarFire SoC Discovery Kit - lwIP Ethernet Diagnostic Suite & Web Server
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

/* lwIP Stack Headers */
#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
#include "lwip/icmp.h"
#include "lwip/raw.h"
#include "lwip/inet_chksum.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "netif/etharp.h"

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
extern err_t mss_mac_netif_init(struct netif *netif);

#if HTTPD_USE_CUSTOM_FSI
int fs_open_custom(struct fs_file *file, const char *name);
void fs_close_custom(struct fs_file *file);
#endif

/* Buffers & Instance Declarations */
static uint8_t g_mac_rx_buffer[MSS_MAC_RX_RING_SIZE][MSS_MAC_MAX_RX_BUF_SIZE] __attribute__((aligned(16)));
static mss_mac_cfg_t g_mac_config;
mss_mac_instance_t *g_test_mac = &g_mac0;

/* lwIP State Declarations */
struct netif g_netif;
volatile uint32_t g_tick_counter = 0;
static uint64_t g_link_status_timer = 0;
static volatile uint8_t g_test_linkup = 0;
static uint8_t g_test_fullduplex = 0;
static mss_mac_speed_t g_test_speed = MSS_MAC_1000MBPS;
static uint8_t g_phy_addr = PHY_VSC8221_MDIO_ADDR; /* 11U */
static uint8_t g_board_ip[4] = {192, 168, 20, 107};

/* Packet Capture Diagnostics */
static volatile uint64_t g_tx_count = 0;
static volatile uint64_t g_rx_count = 0;
static volatile int g_capture = PACKET_IDLE;
static uint8_t g_packet_data[PACKET_MAX];
static volatile uint32_t g_packet_length = 0;
static bool g_auto_rearm = false;
static uint8_t g_last_tx_buf[PACKET_MAX] __attribute__((aligned(8)));
static uint32_t g_last_tx_len = 0;

/* Ping RAW PCB Pointer */
static struct raw_pcb *g_ping_pcb = NULL;
static u16_t g_ping_seq_num = 0;

/* lwIP System Protection Hooks */
sys_prot_t sys_arch_protect(void) {
    __disable_irq();
    return 1;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void)pval;
    __enable_irq();
}

/* Returns current system time in milliseconds for lwIP timers */
u32_t sys_now(void) {
    return g_tick_counter;
}

/* File System Wrapper Functions required by lwIP httpd in Custom FSI Mode */
err_t fs_open(struct fs_file *file, const char *name) {
    if (file == NULL || name == NULL) {
        return ERR_ARG;
    }
    memset(file, 0, sizeof(struct fs_file));
#if HTTPD_USE_CUSTOM_FSI
    if (fs_open_custom(file, name)) {
        return ERR_OK;
    }
#endif
    return ERR_VAL;
}

void fs_close(struct fs_file *file) {
    if (file != NULL) {
#if HTTPD_USE_CUSTOM_FSI
        fs_close_custom(file);
#endif
    }
}

int fs_read(struct fs_file *file, char *buffer, int count) {
    if (file == NULL || buffer == NULL || count <= 0) {
        return -1;
    }
    int read_len = file->len - file->index;
    if (read_len > count) {
        read_len = count;
    }
    if (read_len > 0) {
        memcpy(buffer, file->data + file->index, read_len);
        file->index += read_len;
        return read_len;
    }
    return -1;
}

int fs_bytes_left(struct fs_file *file) {
    if (file == NULL) {
        return 0;
    }
    return file->len - file->index;
}

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

/* Transmit Complete Interrupt Callback */
static void packet_tx_complete_handler(void *this_mac, uint32_t queue_no, mss_mac_tx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)this_mac; (void)queue_no;
    g_tx_count++;
}

/* Receive Interrupt Callback: Forwards Frame into lwIP Input Queue */
static void mac_rx_callback(void *this_mac, uint32_t queue_no, uint8_t *p_rx_packet, uint32_t pckt_length, mss_mac_rx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)queue_no;

    if (pckt_length > 0 && pckt_length <= PACKET_MAX) {
        /* Capture diagnostic hook */
        if (PACKET_ARMED == g_capture) {
            memcpy(g_packet_data, p_rx_packet, pckt_length);
            g_packet_length = pckt_length;
            g_capture = PACKET_DONE;
        }

        /* Pass incoming frame directly to lwIP stack */
        struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)pckt_length, PBUF_POOL);
        if (p != NULL) {
            pbuf_take(p, p_rx_packet, (u16_t)pckt_length);
            if (g_netif.input(p, &g_netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }

    MSS_MAC_receive_pkt((mss_mac_instance_t *)this_mac, 0, p_rx_packet, 0, 1);
    g_rx_count++;
}

/* Dynamic HTTP File Handler Hook for lwIP HTTPD */
#if HTTPD_USE_CUSTOM_FSI
int fs_open_custom(struct fs_file *file, const char *name) {
    if (strcmp(name, "/api/data") == 0) {
        uint32_t gem_ctrl = gem_read_reg(0x000);
        uint32_t gem_cfg  = gem_read_reg(0x004);
        uint32_t gem_stat = gem_read_reg(0x008);
        uint32_t gem_tx   = gem_read_reg(0x014);
        uint32_t gem_rx_q = gem_read_reg(0x018);
        uint32_t gem_tx_q = gem_read_reg(0x01C);
        uint32_t gem_rx   = gem_read_reg(0x020);

        uint16_t phy_bmcr   = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_BMCR);
        uint16_t phy_bmsr   = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_BMSR);
        uint16_t phy_id1    = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_PHYSID1);
        uint16_t phy_id2    = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_PHYSID2);
        uint16_t phy_adv    = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_ADVERTISE);
        uint16_t phy_ctrl1k = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_CTRL1000);
        uint16_t phy_stat1k = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_STAT1000);

        uint32_t tx_oct_lo = gem_read_reg(0x100);
        uint32_t rx_oct_lo = gem_read_reg(0x118);
        uint32_t crc_err   = gem_read_reg(0x14C);
        uint32_t align_err = gem_read_reg(0x150);
        uint32_t overrun   = gem_read_reg(0x164);

        static char json_buf[750];
        snprintf(json_buf, sizeof(json_buf),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"
            "{\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"ip\":\"%s\","
            "\"link\":\"%s\",\"speed\":\"%s\",\"duplex\":\"%s\","
            "\"tx_cnt\":%lu,\"rx_cnt\":%lu,"
            "\"gem\":[\"0x%08X\",\"0x%08X\",\"0x%08X\",\"0x%08X\",\"0x%08X\",\"0x%08X\",\"0x%08X\"],"
            "\"phy\":[\"0x%04X\",\"0x%04X\",\"0x%04X:0x%04X\",\"0x%04X\",\"0x%04X/0x%04X\"],"
            "\"tx_oct\":\"%lu\",\"rx_oct\":\"%lu\",\"crc\":%u,\"align\":%u,\"ovr\":%u}",
            g_test_mac->mac_addr[0], g_test_mac->mac_addr[1], g_test_mac->mac_addr[2],
            g_test_mac->mac_addr[3], g_test_mac->mac_addr[4], g_test_mac->mac_addr[5],
            ip4addr_ntoa(&g_netif.ip_addr),
            g_test_linkup ? "UP" : "DOWN",
            (g_test_speed == MSS_MAC_1000MBPS) ? "1Gbps" : "100Mbps",
            g_test_fullduplex ? "Full" : "Half",
            (unsigned long)g_tx_count, (unsigned long)g_rx_count,
            (unsigned int)gem_ctrl, (unsigned int)gem_cfg, (unsigned int)gem_stat,
            (unsigned int)gem_tx, (unsigned int)gem_rx_q, (unsigned int)gem_tx_q, (unsigned int)gem_rx,
            (unsigned int)phy_bmcr, (unsigned int)phy_bmsr,
            (unsigned int)phy_id1, (unsigned int)phy_id2,
            (unsigned int)phy_adv, (unsigned int)phy_ctrl1k, (unsigned int)phy_stat1k,
            (unsigned long)tx_oct_lo, (unsigned long)rx_oct_lo,
            (unsigned int)crc_err, (unsigned int)align_err, (unsigned int)overrun);

        file->data = json_buf;
        file->len = (int)strlen(json_buf);
        file->index = 0;
        file->pextension = NULL;
        return 1;
    }

    if (strcmp(name, "/") == 0 || strcmp(name, "/index.html") == 0) {
        static const char *ui_html =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
            "<!DOCTYPE html><html><head><title>PolarFire SoC</title>"
            "<style>body{font:12px monospace;background:#0d1117;color:#c9d1d9;padding:15px}"
            "h3{color:#58a6ff;margin:10px 0 4px 0}.card{background:#161b22;padding:10px;border-radius:6px;border:1px solid #30363d;margin-bottom:10px}"
            "table{border-collapse:collapse;width:100%;margin-bottom:10px}td,th{padding:4px;border:1px solid #30363d;text-align:left}</style></head><body>"
            "<h3>PolarFire SoC GEM0 System Dashboard (lwIP Stack)</h3>"
            "<div class=\"card\" id=\"meta\">Loading...</div>"
            "<h3>GEM0 MAC Registers</h3>"
            "<table><tr><th>Register</th><th>Offset</th><th>Value</th></tr>"
            "<tr><td>NET_CTRL / NET_CFG / NET_STAT</td><td>0x000..08</td><td id=\"g0\">-</td></tr>"
            "<tr><td>TX_STAT / RX_STAT</td><td>0x014 / 0x020</td><td id=\"g1\">-</td></tr>"
            "<tr><td>RX_Q_PTR / TX_Q_PTR</td><td>0x018 / 0x01C</td><td id=\"g2\">-</td></tr></table>"
            "<h3>VSC8221 PHY Registers</h3>"
            "<table><tr><th>Register</th><th>Reg</th><th>Value</th></tr>"
            "<tr><td>BMCR / BMSR</td><td>0x00 / 0x01</td><td id=\"p0\">-</td></tr>"
            "<tr><td>PHYSID1 / PHYSID2</td><td>0x02 / 0x03</td><td id=\"p1\">-</td></tr>"
            "<tr><td>AN_ADV / 1000 CTRL/STAT</td><td>0x04 / 0x09..0A</td><td id=\"p2\">-</td></tr></table>"
            "<h3>Hardware MIB Statistics</h3>"
            "<table><tr><th>Metric</th><th>Value</th></tr>"
            "<tr><td>TX Octets / RX Octets</td><td id=\"s0\">-</td></tr>"
            "<tr><td>CRC Errs / Align Errs / Overruns</td><td id=\"s1\">-</td></tr></table>"
            "<script>function update(){fetch('/api/data').then(r=>r.json()).then(d=>{"
            "document.getElementById('meta').innerHTML='<b>MAC:</b> '+d.mac+' | <b>IP:</b> '+d.ip+' | <b>Link:</b> '+d.link+' ('+d.speed+' '+d.duplex+') | <b>TX/RX Pkts:</b> '+d.tx_cnt+' / '+d.rx_cnt;"
            "document.getElementById('g0').innerText=d.gem[0]+' / '+d.gem[1]+' / '+d.gem[2];"
            "document.getElementById('g1').innerText=d.gem[3]+' / '+d.gem[6];"
            "document.getElementById('g2').innerText=d.gem[4]+' / '+d.gem[5];"
            "document.getElementById('p0').innerText=d.phy[0]+' / '+d.phy[1];"
            "document.getElementById('p1').innerText=d.phy[2];"
            "document.getElementById('p2').innerText=d.phy[3]+' / '+d.phy[4];"
            "document.getElementById('s0').innerText=d.tx_oct+' B / '+d.rx_oct+' B';"
            "document.getElementById('s1').innerText=d.crc+' / '+d.align+' / '+d.ovr;"
            "});}update();setInterval(update,2000);</script></body></html>";

        file->data = ui_html;
        file->len = (int)strlen(ui_html);
        file->index = 0;
        file->pextension = NULL;
        return 1;
    }
    return 0;
}

void fs_close_custom(struct fs_file *file) {
    (void)file;
}
#endif

/* Low Level Ethernet Driver Init */
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

/* Initialize lwIP Stack and Protocols */
static void lwip_network_init(void) {
    ip4_addr_t ipaddr, netmask, gw;

    IP4_ADDR(&ipaddr, g_board_ip[0], g_board_ip[1], g_board_ip[2], g_board_ip[3]);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, g_board_ip[0], g_board_ip[1], g_board_ip[2], 1);

    lwip_init();

    netif_add(&g_netif, &ipaddr, &netmask, &gw,
              g_test_mac,
              mss_mac_netif_init,
              ethernet_input);

    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    httpd_init();
    dhcp_start(&g_netif);

    PRINT_STRING("[lwIP] Stack, HTTPD Web Server, and DHCP initialized successfully.\r\n");
}

/* lwIP Ping Engine (RAW API) */
static u8_t lwip_ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    struct icmp_echo_hdr *iecho;
    LWIP_UNUSED_ARG(arg); LWIP_UNUSED_ARG(pcb);

    if (p->tot_len >= (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr))) {
        iecho = (struct icmp_echo_hdr *)((u8_t *)p->payload + PBUF_IP_HLEN);
        if (ICMPH_TYPE(iecho) == ICMP_ER) {
            char info[128];
            sprintf(info, "[PING] Reply received from %s\r\n", ipaddr_ntoa(addr));
            PRINT_STRING(info);
            pbuf_free(p);
            return 1;
        }
    }
    return 0;
}

static void send_lwip_ping(const ip_addr_t *target_addr) {
    if (g_ping_pcb == NULL) {
        g_ping_pcb = raw_new(IP_PROTO_ICMP);
        raw_recv(g_ping_pcb, lwip_ping_recv, NULL);
        raw_bind(g_ping_pcb, IP_ADDR_ANY);
    }

    struct pbuf *p;
    struct icmp_echo_hdr *iecho;
    size_t ping_size = sizeof(struct icmp_echo_hdr) + 32;

    p = pbuf_alloc(PBUF_RAW, (u16_t)ping_size, PBUF_RAM);
    if (!p) return;

    iecho = (struct icmp_echo_hdr *)p->payload;
    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id     = 0xAFAF;
    iecho->seqno  = lwip_htons(++g_ping_seq_num);

    memset((char *)iecho + sizeof(struct icmp_echo_hdr), 'a', 32);
    iecho->chksum = inet_chksum(iecho, ping_size);

    raw_sendto(g_ping_pcb, p, target_addr);
    pbuf_free(p);

    char info[128];
    sprintf(info, "[PING] Transmitted ICMP Echo Request to %s\r\n", ipaddr_ntoa(target_addr));
    PRINT_STRING(info);
}

static void set_board_mac_address(uint8_t m1, uint8_t m2, uint8_t m3, uint8_t m4, uint8_t m5, uint8_t m6) {
    g_test_mac->mac_addr[0] = m1;
    g_test_mac->mac_addr[1] = m2;
    g_test_mac->mac_addr[2] = m3;
    g_test_mac->mac_addr[4] = m5;
    g_test_mac->mac_addr[5] = m6;

    uint32_t mac_bottom = (uint32_t)m1 | ((uint32_t)m2 << 8) | ((uint32_t)m3 << 16) | ((uint32_t)m4 << 24);
    uint32_t mac_top = (uint32_t)m5 | ((uint32_t)m6 << 8);

    g_test_mac->mac_base->SPEC_ADD1_BOTTOM = mac_bottom;
    g_test_mac->mac_base->SPEC_ADD1_TOP = mac_top;
    memcpy(g_netif.hwaddr, g_test_mac->mac_addr, 6);

    char info[128];
    sprintf(info, "[MAC] Hardware MAC address changed to %02X:%02X:%02X:%02X:%02X:%02X\r\n",
            (unsigned int)m1, (unsigned int)m2, (unsigned int)m3,
            (unsigned int)m4, (unsigned int)m5, (unsigned int)m6);
    PRINT_STRING(info);
}

static void set_board_ip_address(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4) {
    g_board_ip[0] = ip1; g_board_ip[1] = ip2; g_board_ip[2] = ip3; g_board_ip[3] = ip4;

    ip4_addr_t new_ip, netmask, gw;
    IP4_ADDR(&new_ip, ip1, ip2, ip3, ip4);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, ip1, ip2, ip3, 1);

    netif_set_addr(&g_netif, &new_ip, &netmask, &gw);

    char info[128];
    sprintf(info, "[IP] Board IPv4 address changed to %u.%u.%u.%u\r\n", ip1, ip2, ip3, ip4);
    PRINT_STRING(info);
}

static int send_ethernet_frame(const uint8_t *buf, uint32_t len) {
    if (len > PACKET_MAX) len = PACKET_MAX;
    memcpy(g_last_tx_buf, buf, len);
    g_last_tx_len = len;

    int32_t st = MSS_MAC_send_pkt(g_test_mac, 0, g_last_tx_buf, len, NULL);
    return (int)st;
}

static void print_help(void) {
    PRINT_STRING("\r\n================ Diagnostic & Test Suite CLI (lwIP Stack) ================\r\n");
    PRINT_STRING("  status                        - Display MAC, IP, link & packet counters\r\n");
    PRINT_STRING("  stats                         - Display detailed MAC MIB statistics\r\n");
    PRINT_STRING("  stats clear                   - Clear hardware MAC statistics\r\n");
    PRINT_STRING("  ping <ip1>.<ip2>.<ip3>.<ip4>  - Send ICMP Echo Request to host IP\r\n");
    PRINT_STRING("  promisc                       - Toggle GEM Promiscuous Mode\r\n");
    PRINT_STRING("  set ip <ip1>.<ip2>.<ip3>.<ip4>- Manually change board IPv4 address\r\n");
    PRINT_STRING("  set mac <XX:XX:XX:XX:XX:XX>   - Manually change hardware MAC address\r\n");
    PRINT_STRING("  loopback pcs                  - Toggle GEM PCS SGMII Loopback\r\n");
    PRINT_STRING("  loopback phy                  - Toggle VSC8221 PHY Loopback\r\n");
    PRINT_STRING("  loopback off                  - Disable all loopback modes\r\n");
    PRINT_STRING("  capture                       - Arm single-packet capture\r\n");
    PRINT_STRING("  monitor                       - Toggle continuous packet monitoring\r\n");
    PRINT_STRING("  rx inspect                    - Detailed layer-by-layer frame decoder\r\n");
    PRINT_STRING("  rx flush                      - Clear and re-arm RX capture engine\r\n");
    PRINT_STRING("  packet diff [bytes]           - Side-by-side TX vs RX buffer comparison\r\n");
    PRINT_STRING("  desc inspect                  - Inspect DMA descriptor queue pointers\r\n");
    PRINT_STRING("  mactest <len> <count>         - Run automated Layer 2 pattern test\r\n");
    PRINT_STRING("  tx raw <len> [pat_hex]        - Transmit custom L2 raw frame\r\n");
    PRINT_STRING("  tx arp                        - Transmit sample ARP frame\r\n");
    PRINT_STRING("  dhcp                          - Re-trigger DHCP Discover client\r\n");
    PRINT_STRING("  speed <10|100|1000> <half|full> - Force MAC/PHY speed & duplex\r\n");
    PRINT_STRING("  phy reset                     - Issue software reset to VSC8221 PHY\r\n");
    PRINT_STRING("  mdio scan                     - Scan MDIO bus (0-31) for PHYs\r\n");
    PRINT_STRING("  mdio read <reg_hex>           - Read 16-bit PHY register\r\n");
    PRINT_STRING("  mdio write <r> <v>            - Write 16-bit PHY register\r\n");
    PRINT_STRING("  phy dump                      - Dump PHY registers (0x00-0x0F)\r\n");
    PRINT_STRING("  gem dump                      - Dump GEM0 MAC registers (0x000-0x084)\r\n");
    PRINT_STRING("  help                          - Print Help Menu\r\n");
    PRINT_STRING("===========================================================================\r\n\r\n");
}

static void print_mac_statistics(void) {
    char info[128];
    uint32_t tx_octets_lo = gem_read_reg(0x100);
    uint32_t tx_octets_hi = gem_read_reg(0x104);
    uint32_t tx_frames    = gem_read_reg(0x108);
    uint32_t rx_octets_lo = gem_read_reg(0x118);
    uint32_t rx_octets_hi = gem_read_reg(0x11C);
    uint32_t rx_frames    = gem_read_reg(0x120);
    uint32_t fcs_errors   = gem_read_reg(0x14C);
    uint32_t align_errors = gem_read_reg(0x150);
    uint32_t rx_overruns  = gem_read_reg(0x164);

    uint64_t tx_octets = ((uint64_t)tx_octets_hi << 32) | tx_octets_lo;
    uint64_t rx_octets = ((uint64_t)rx_octets_hi << 32) | rx_octets_lo;

    PRINT_STRING("\r\n================ Hard GEM0 Hardware MIB Statistics ================\r\n");
    sprintf(info, " Frames Transmitted OK : %u (Driver Counter: %lu)\r\n", (unsigned int)tx_frames, (unsigned long)g_tx_count); PRINT_STRING(info);
    sprintf(info, " Octets Transmitted OK : %llu\r\n", (unsigned long long)tx_octets); PRINT_STRING(info);
    sprintf(info, " Frames Received OK    : %u (Driver Counter: %lu)\r\n", (unsigned int)rx_frames, (unsigned long)g_rx_count); PRINT_STRING(info);
    sprintf(info, " Octets Received OK    : %llu\r\n", (unsigned long long)rx_octets); PRINT_STRING(info);
    sprintf(info, " CRC / FCS Errors      : %u\r\n", (unsigned int)fcs_errors); PRINT_STRING(info);
    sprintf(info, " Alignment Errors      : %u\r\n", (unsigned int)align_errors); PRINT_STRING(info);
    sprintf(info, " RX Overrun Errors     : %u\r\n", (unsigned int)rx_overruns); PRINT_STRING(info);
    PRINT_STRING("===================================================================\r\n\r\n");
}

static void toggle_promiscuous_mode(void) {
    volatile uint32_t cfg = g_test_mac->mac_base->NETWORK_CONFIG;
    if (cfg & (1U << 4)) {
        g_test_mac->mac_base->NETWORK_CONFIG &= ~(1U << 4);
        PRINT_STRING("[MAC] Promiscuous Mode (Copy All Frames) DISABLED\r\n");
    } else {
        g_test_mac->mac_base->NETWORK_CONFIG |= (1U << 4);
        PRINT_STRING("[MAC] Promiscuous Mode (Copy All Frames) ENABLED\r\n");
    }
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

static void send_custom_raw_packet(uint32_t len, uint8_t pattern, bool use_pattern) {
    static uint8_t raw_frame[PACKET_MAX];

    if (len < ETH_MIN_FRAME_LEN) len = ETH_MIN_FRAME_LEN;
    if (len > PACKET_MAX) len = PACKET_MAX;

    memset(&raw_frame[0], 0xFF, 6);
    memcpy(&raw_frame[6], g_test_mac->mac_addr, 6);
    raw_frame[12] = 0x88; raw_frame[13] = 0xB5;

    for (uint32_t i = 14; i < len; i++) {
        raw_frame[i] = use_pattern ? pattern : (uint8_t)(i & 0xFF);
    }

    char info[128];
    sprintf(info, "[TX RAW] Transmitting %u byte raw frame (EtherType 0x88B5)...\r\n", (unsigned int)len);
    PRINT_STRING(info);

    send_ethernet_frame(raw_frame, len);
}

static void run_mac_pattern_test(uint32_t len, uint32_t count) {
    char info[128];
    uint32_t pass = 0, fail = 0;

    if (len < ETH_MIN_FRAME_LEN) len = ETH_MIN_FRAME_LEN;
    if (len > PACKET_MAX) len = PACKET_MAX;

    static uint8_t tx_frame[PACKET_MAX];
    memset(&tx_frame[0], 0xFF, 6);
    memcpy(&tx_frame[6], g_test_mac->mac_addr, 6);
    tx_frame[12] = 0x88; tx_frame[13] = 0xB5;

    for (uint32_t i = 14; i < len; i++) {
        tx_frame[i] = (uint8_t)(i & 0xFF);
    }

    sprintf(info, "\r\n[MAC Test] Running automated loopback test (%u frames, %u bytes each)...\r\n",
            (unsigned int)count, (unsigned int)len);
    PRINT_STRING(info);

    bool prev_rearm = g_auto_rearm;
    g_auto_rearm = false;

    for (uint32_t i = 0; i < count; i++) {
        g_packet_length = 0;
        g_capture = PACKET_ARMED;

        send_ethernet_frame(tx_frame, len);

        uint32_t timeout = 100000;
        while (g_capture != PACKET_DONE && --timeout) {
            for (volatile int d = 0; d < 10; d++);
        }

        if (g_capture == PACKET_DONE && g_packet_length == len) {
            if (memcmp(tx_frame, g_packet_data, len) == 0) {
                pass++;
            } else {
                sprintf(info, "  [Frame %u] Data Mismatch Error!\r\n", (unsigned int)(i + 1));
                PRINT_STRING(info);
                fail++;
            }
        } else {
            sprintf(info, "  [Frame %u] Timeout or Length Error (Exp: %u, Got: %u)\r\n",
                    (unsigned int)(i + 1), (unsigned int)len, (unsigned int)g_packet_length);
            PRINT_STRING(info);
            fail++;
        }
    }

    g_auto_rearm = prev_rearm;
    g_capture = PACKET_IDLE;

    sprintf(info, "[MAC Test] Complete: PASS = %u, FAIL = %u\r\n\r\n", (unsigned int)pass, (unsigned int)fail);
    PRINT_STRING(info);
}

static void reset_phy_hardware(void) {
    PRINT_STRING("[PHY] Resetting VSC8221 PHY via BMCR Bit 15...\r\n");
    uint16_t bmcr = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_BMCR);
    MSS_MAC_write_phy_reg(g_test_mac, g_phy_addr, MII_BMCR, (uint16_t)(bmcr | BMCR_RESET));

    for (volatile int i = 0; i < 5000000; i++);

    g_test_mac->phy_autonegotiate(g_test_mac);
    PRINT_STRING("[PHY] Reset complete. Auto-negotiation re-started.\r\n");
}

static void set_speed_duplex(uint32_t mbps, bool full_duplex) {
    mss_mac_speed_mode_t spd_mode = MSS_MAC_1000_FDX;
    char info[128];

    if (mbps == 10) {
        spd_mode = full_duplex ? MSS_MAC_10_FDX : MSS_MAC_10_HDX;
    } else if (mbps == 100) {
        spd_mode = full_duplex ? MSS_MAC_100_FDX : MSS_MAC_100_HDX;
    } else if (mbps == 1000) {
        spd_mode = full_duplex ? MSS_MAC_1000_FDX : MSS_MAC_1000_HDX;
    }

    MSS_MAC_change_speed(g_test_mac, g_test_mac->speed_duplex_select, spd_mode);
    sprintf(info, "[SPEED] Speed changed to %uMbps (%s Duplex)\r\n",
            (unsigned int)mbps, full_duplex ? "Full" : "Half");
    PRINT_STRING(info);
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
        sprintf(info, "Dst MAC   : %02X:%02X:%02X:%02X:%02X:%02X\r\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]); PRINT_STRING(info);
        sprintf(info, "Src MAC   : %02X:%02X:%02X:%02X:%02X:%02X\r\n", buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]); PRINT_STRING(info);

        uint16_t ethertype = (uint16_t)(((uint16_t)buf[12] << 8) | buf[13]);
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
                uint16_t opcode = (uint16_t)(((uint16_t)buf[20] << 8) | buf[21]);
                sprintf(info, "  |- Opcode   : %s (%u)\r\n", (opcode == 1) ? "Request" : (opcode == 2) ? "Reply" : "Unknown", opcode); PRINT_STRING(info);
                sprintf(info, "  |- Sender IP: %u.%u.%u.%u\r\n", buf[28], buf[29], buf[30], buf[31]); PRINT_STRING(info);
                sprintf(info, "  \\- Target IP: %u.%u.%u.%u\r\n", buf[38], buf[39], buf[40], buf[41]); PRINT_STRING(info);
            }
        } else if (ethertype == 0x88B5) {
            PRINT_STRING("EtherType : 0x88B5 (Custom Test Frame)\r\n");
        } else {
            sprintf(info, "EtherType : 0x%04X\r\n", ethertype); PRINT_STRING(info);
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
    char str_arg[32] = {0};
    ip4_addr_t last_dhcp_ip = {0};

    SYSREG->SOFT_RESET_CR = 0U;
    SYSREG->SUBBLK_CLOCK_CR = 0xFFFFFFFFUL;

    __disable_local_irq((int8_t)MMUART0_E51_INT);
    SysTick_Config();

    MSS_UART_init(DEMO_UART, MSS_UART_115200_BAUD, MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);
    PRINT_STRING("\r\n***** PolarFire SoC Discovery Kit Diagnostic Suite (lwIP Stack Integration) *****\r\n");
    __enable_irq();

    low_level_init();
    lwip_network_init();
    print_help();

    PRINT_STRING("eth-disco> ");

    while (1) {
        /* Process link status polling */
        prvLinkStatusTask();

        /* Process lwIP Internal Timers (DHCP, TCP, ARP) */
        sys_check_timeouts();

        /* Dynamic DHCP IP Notification */
        if (dhcp_supplied_address(&g_netif) && (g_netif.ip_addr.addr != last_dhcp_ip.addr)) {
            last_dhcp_ip = g_netif.ip_addr;
            sprintf(info, "\r\n[DHCP SUCCESS] Dynamic IP Assigned: %s\r\neth-disco> ", ip4addr_ntoa(&g_netif.ip_addr));
            PRINT_STRING(info);
        }

        if (PACKET_DONE == g_capture) {
            packet_dump();
            PRINT_STRING("eth-disco> ");
        }

        if (get_cli_line(cli_input, sizeof(cli_input))) {
            char *cmd = cli_input;
            while (*cmd == ' ') cmd++;

            if (strcmp(cmd, "status") == 0) {
                sprintf(info, "[STATUS] MAC Addr: %02X:%02X:%02X:%02X:%02X:%02X | IP Addr: %s\r\n",
                        (unsigned int)g_test_mac->mac_addr[0], (unsigned int)g_test_mac->mac_addr[1],
                        (unsigned int)g_test_mac->mac_addr[2], (unsigned int)g_test_mac->mac_addr[3],
                        (unsigned int)g_test_mac->mac_addr[4], (unsigned int)g_test_mac->mac_addr[5],
                        ip4addr_ntoa(&g_netif.ip_addr));
                PRINT_STRING(info);
                sprintf(info, "[STATUS] Link: %s | Speed: %s | Duplex: %s\r\n",
                        g_test_linkup ? "UP" : "DOWN",
                        (g_test_speed == MSS_MAC_1000MBPS) ? "1Gbps" : "100Mbps",
                        g_test_fullduplex ? "Full" : "Half");
                PRINT_STRING(info);
                sprintf(info, "[STATUS] Transmitted Packets: %lu | Received Packets: %lu\r\n",
                        (unsigned long)g_tx_count, (unsigned long)g_rx_count);
                PRINT_STRING(info);
            }
            else if (strcmp(cmd, "stats clear") == 0) {
                MSS_MAC_clear_statistics(g_test_mac);
                g_tx_count = 0; g_rx_count = 0;
                PRINT_STRING("[STATS] Hardware MIB counters cleared.\r\n");
            }
            else if (strcmp(cmd, "stats") == 0) {
                print_mac_statistics();
            }
            else if (strncmp(cmd, "ping ", 5) == 0) {
                uint32_t ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
                if (sscanf(cmd + 5, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4) {
                    ip4_addr_t ping_addr;
                    IP4_ADDR(&ping_addr, ip1, ip2, ip3, ip4);
                    send_lwip_ping(&ping_addr);
                } else {
                    PRINT_STRING("Usage: ping <ip1>.<ip2>.<ip3>.<ip4>\r\n");
                }
            }
            else if (strcmp(cmd, "promisc") == 0) {
                toggle_promiscuous_mode();
            }
            else if (strncmp(cmd, "set ip ", 7) == 0) {
                uint32_t ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
                if (sscanf(cmd + 7, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4) {
                    set_board_ip_address((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4);
                } else {
                    PRINT_STRING("Usage: set ip <ip1>.<ip2>.<ip3>.<ip4>\r\n");
                }
            }
            else if (strncmp(cmd, "set mac ", 8) == 0) {
                uint32_t m1 = 0, m2 = 0, m3 = 0, m4 = 0, m5 = 0, m6 = 0;
                if (sscanf(cmd + 8, "%x:%x:%x:%x:%x:%x", &m1, &m2, &m3, &m4, &m5, &m6) == 6) {
                    set_board_mac_address((uint8_t)m1, (uint8_t)m2, (uint8_t)m3,
                                          (uint8_t)m4, (uint8_t)m5, (uint8_t)m6);
                } else {
                    PRINT_STRING("Usage: set mac <XX:XX:XX:XX:XX:XX>\r\n");
                }
            }
            else if (strcmp(cmd, "loopback pcs") == 0 || strcmp(cmd, "pcs loopback") == 0) {
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
                g_test_mac->mac_base->PCS_CONTROL &= ~GEM_LOOPBACK_MODE;
                g_test_mac->mac_base->NETWORK_CONTROL &= ~(GEM_LOOPBACK_LOCAL | GEM_LOOPBACK);
                uint16_t bmcr = MSS_MAC_read_phy_reg(g_test_mac, g_phy_addr, MII_BMCR);
                MSS_MAC_write_phy_reg(g_test_mac, g_phy_addr, MII_BMCR, (uint16_t)(bmcr & ~(uint16_t)BMCR_LOOPBACK));
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
            else if (strcmp(cmd, "rx flush") == 0 || strcmp(cmd, "rx clear") == 0) {
                g_packet_length = 0;
                g_capture = PACKET_IDLE;
                PRINT_STRING("[RX] Capture engine flushed and reset to IDLE.\r\n");
            }
            else if (sscanf(cmd, "packet diff %u", &p1) == 1 || strcmp(cmd, "packet diff") == 0) {
                compare_tx_rx_buffers(p1);
            }
            else if (strcmp(cmd, "desc inspect") == 0 || strcmp(cmd, "dma status") == 0) {
                inspect_dma_descriptors();
            }
            else if (sscanf(cmd, "mactest %u %u", &p1, &p2) == 2) {
                run_mac_pattern_test(p1, p2);
            }
            else if (strncmp(cmd, "tx raw ", 7) == 0) {
                uint32_t len = 64, pat = 0;
                if (sscanf(cmd, "tx raw %u %x", &len, &pat) == 2) {
                    send_custom_raw_packet(len, (uint8_t)pat, true);
                } else if (sscanf(cmd, "tx raw %u", &len) == 1) {
                    send_custom_raw_packet(len, 0, false);
                } else {
                    PRINT_STRING("Usage: tx raw <length_bytes> [pattern_hex]\r\n");
                }
            }
            else if (strcmp(cmd, "tx arp") == 0 || strcmp(cmd, "tx") == 0) {
                uint8_t arp_frame[60];
                memset(arp_frame, 0, sizeof(arp_frame));
                memset(&arp_frame[0], 0xFF, 6);
                memcpy(&arp_frame[6], g_test_mac->mac_addr, 6);
                arp_frame[12] = 0x08; arp_frame[13] = 0x06;
                arp_frame[14] = 0x00; arp_frame[15] = 0x01;
                arp_frame[16] = 0x08; arp_frame[17] = 0x00;
                arp_frame[18] = 0x06; arp_frame[19] = 0x04;
                arp_frame[20] = 0x00; arp_frame[21] = 0x01;
                memcpy(&arp_frame[22], g_test_mac->mac_addr, 6);
                memcpy(&arp_frame[28], g_board_ip, 4);
                memset(&arp_frame[32], 0x00, 6);
                arp_frame[38] = 192; arp_frame[39] = 168; arp_frame[40] = 20; arp_frame[41] = 1;

                int st = send_ethernet_frame(arp_frame, sizeof(arp_frame));
                sprintf(info, "[CMD] Transmitted ARP Packet via MAC. Driver Status: %d\r\n", st);
                PRINT_STRING(info);
            }
            else if (strcmp(cmd, "dhcp") == 0) {
                PRINT_STRING("[DHCP] Restarting dynamic DHCP client...\r\n");
                dhcp_stop(&g_netif);
                dhcp_start(&g_netif);
            }
            else if (sscanf(cmd, "speed %u %s", &p1, str_arg) == 2) {
                bool is_full = (strcmp(str_arg, "full") == 0);
                set_speed_duplex(p1, is_full);
            }
            else if (strcmp(cmd, "phy reset") == 0) {
                reset_phy_hardware();
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
                MSS_MAC_write_phy_reg(g_test_mac, g_phy_addr, (uint8_t)p1, (uint16_t)(p2 & 0xFFFFU));
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
