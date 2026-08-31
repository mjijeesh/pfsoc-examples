/*******************************************************************************
 * UART Subsystem: Interactive CLI, Diagnostic Commands & Callbacks
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "uart_cli.h"
#include "app_shared.h"

#include "lwip/dhcp.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/etharp.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"

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

static u8_t ping_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    (void)arg; (void)pcb;
    if (p->tot_len >= (20 + sizeof(struct icmp_echo_hdr))) {
        struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)((u8_t *)p->payload + 20);
        if (ICMPH_TYPE(iecho) == ICMP_ER) {
            char buf[128];
            sprintf(buf, "[DEBUG] ICMP Echo Reply received from %s (Seq: %u)\r\neth-cli> ",
                    ipaddr_ntoa(addr), lwip_ntohs(iecho->seqno));
            log_msg(buf);
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
    log_msg(dbg);

    raw_sendto(ping_pcb, p, &target);
    pbuf_free(p);
}

static void send_lwip_arp_request(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4) {
    ip4_addr_t target_ip;
    IP4_ADDR(&target_ip, ip1, ip2, ip3, ip4);

    char dbg[128];
    sprintf(dbg, "[DEBUG] Transmitting Manual ARP Request for %u.%u.%u.%u\r\n", ip1, ip2, ip3, ip4);
    log_msg(dbg);

    etharp_request(&g_netif, &target_ip);
}

void my_iperf_report_cb(void *arg, enum lwiperf_report_type report_type,
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
    log_msg(buf);
}

void print_help(void) {
    log_msg("\r\n======================== Available CLI Commands ========================\r\n");
    log_msg("  help                   - Display this command documentation menu\r\n");
    log_msg("  status                 - Show Network Interface details, MAC, IP & Packets\r\n");
    log_msg("  monitor <on|off>       - Enable or disable background packet debug output\r\n");
    log_msg("  dhcp                   - Initiate DHCP DISCOVER to request dynamic IP\r\n");
    log_msg("  ping <ip1.ip2.ip3.ip4> - Send outbound ICMP Echo Request to target host\r\n");
    log_msg("  arp <ip1.ip2.ip3.ip4>  - Send manual Layer 2 ARP Request for target IP\r\n");
    log_msg("------------------------------------------------------------------------\r\n");
    log_msg("  [Active Background Services]\r\n");
    log_msg("  HTTP Web Server        - Port 80   (Open http://<board_ip> in browser)\r\n");
    log_msg("  iPerf Bandwidth Server - Port 5001 (Run iperf test from host PC)\r\n");
    log_msg("========================================================================\r\n");
}

void print_status(void) {
    char info[400];
    char ip_str[16], nm_str[16], gw_str[16];

    ipaddr_ntoa_r(&g_netif.ip_addr, ip_str, sizeof(ip_str));
    ipaddr_ntoa_r(&g_netif.netmask, nm_str, sizeof(nm_str));
    ipaddr_ntoa_r(&g_netif.gw, gw_str, sizeof(gw_str));

    const char *dhcp_status_str = "DISABLED (Static)";
    if (dhcp_supplied_address(&g_netif)) {
        dhcp_status_str = "BOUND (Dynamic IP)";
    } else if (g_dhcp_fallback_active) {
        dhcp_status_str = "FALLBACK (Static IP)";
    } else if (g_dhcp_requested) {
        dhcp_status_str = "DISCOVERING...";
    }

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
                  dhcp_status_str,
                  g_debug_monitor ? "ENABLED (ON)" : "DISABLED (OFF)",
                  (unsigned long)g_tx_count, (unsigned long)g_rx_count);
    log_msg(info);
}

void cli_task(void *pvParameters) {
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
                log_msg("[DEBUG] Real-time traffic monitoring ENABLED.\r\n");
            } else if (strcmp(cmd, "monitor off") == 0) {
                g_debug_monitor = false;
                log_msg("[DEBUG] Real-time traffic monitoring DISABLED.\r\n");
            } else if (strcmp(cmd, "dhcp") == 0) {
                log_msg("[DEBUG] Re-triggering DHCP Client...\r\n");
                g_dhcp_requested = true;
                g_dhcp_bound_reported = false;
                g_dhcp_fallback_active = false;
                dhcp_stop(&g_netif);
                dhcp_start(&g_netif);
            } else if (strncmp(cmd, "arp ", 4) == 0) {
                uint32_t ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
                if (sscanf(cmd + 4, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4) {
                    send_lwip_arp_request((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4);
                } else {
                    log_msg("Usage: arp <ip1>.<ip2>.<ip3>.<ip4>\r\n");
                }
            } else if (strncmp(cmd, "ping ", 5) == 0) {
                uint32_t ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
                if (sscanf(cmd + 5, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4) {
                    send_ping((uint8_t)ip1, (uint8_t)ip2, (uint8_t)ip3, (uint8_t)ip4);
                } else {
                    log_msg("Usage: ping <ip1>.<ip2>.<ip3>.<ip4>\r\n");
                }
            } else if (strlen(cmd) > 0) {
                log_msg("Unknown command. Type 'help' for available CLI commands.\r\n");
            }

            PRINT_STRING("eth-cli> ");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
