/*******************************************************************************
 * Dynamic HTTP Web Dashboard Module
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "app_shared.h"
#include "lwip/apps/fs.h"
#include "lwip/dhcp.h"
#include "FreeRTOS.h"
#include "task.h"

static char g_html_buf[3000];

err_t fs_open(struct fs_file *file, const char *name) {
    (void)name;
    memset(file, 0, sizeof(struct fs_file));

    mss_mac_speed_t speed;
    uint8_t fullduplex;
    uint8_t link_up = MSS_MAC_get_link_status(g_test_mac, &speed, &fullduplex);

    const char *speed_str = "N/A";
    if (link_up) {
        if (speed == MSS_MAC_10MBPS) speed_str = "10 Mbps";
        else if (speed == MSS_MAC_100MBPS) speed_str = "100 Mbps";
        else if (speed == MSS_MAC_1000MBPS) speed_str = "1000 Mbps (1 Gbps)";
    }

    char ip_str[16], nm_str[16], gw_str[16];
    ipaddr_ntoa_r(&g_netif.ip_addr, ip_str, sizeof(ip_str));
    ipaddr_ntoa_r(&g_netif.netmask, nm_str, sizeof(nm_str));
    ipaddr_ntoa_r(&g_netif.gw, gw_str, sizeof(gw_str));

    uint32_t uptime_sec = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);

    snprintf(g_html_buf, sizeof(g_html_buf),
        "<!DOCTYPE html><html><head>"
        "<meta http-equiv='refresh' content='2'>"
        "<title>PolarFire SoC Dashboard</title>"
        "<style>"
        "body{font-family:Segoe UI,Arial,sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:20px;}"
        "h1{color:#38bdf8;border-bottom:2px solid #334155;padding-bottom:10px;margin-top:0;}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:15px;}"
        ".card{background:#1e293b;border-radius:8px;padding:18px;box-shadow:0 4px 6px -1px rgba(0,0,0,0.3);}"
        ".card h2{margin-top:0;color:#f1f5f9;font-size:18px;border-bottom:1px solid #334155;padding-bottom:8px;}"
        "table{width:100%%;border-collapse:collapse;}"
        "td{padding:8px 0;border-bottom:1px solid #334155;font-size:14px;}"
        "td:last-child{text-align:right;font-weight:600;color:#38bdf8;}"
        ".badge-up{background:#166534;color:#4ade80;padding:3px 8px;border-radius:4px;font-size:12px;}"
        ".badge-down{background:#991b1b;color:#f87171;padding:3px 8px;border-radius:4px;font-size:12px;}"
        ".footer{margin-top:20px;font-size:12px;color:#64748b;text-align:center;}"
        "</style></head><body>"
        "<h1>PolarFire SoC Real-Time Network Node</h1>"
        "<div class='grid'>"

        "<div class='card'><h2>MAC & Network Layer</h2><table>"
        "<tr><td>Interface</td><td>%c%c</td></tr>"
        "<tr><td>MAC Address</td><td>%02X:%02X:%02X:%02X:%02X:%02X</td></tr>"
        "<tr><td>IPv4 Address</td><td>%s</td></tr>"
        "<tr><td>Subnet Mask</td><td>%s</td></tr>"
        "<tr><td>Gateway</td><td>%s</td></tr>"
        "<tr><td>DHCP Client</td><td>%s</td></tr>"
        "</table></div>"

        "<div class='card'><h2>PHY Hardware Status</h2><table>"
        "<tr><td>PHY Transceiver</td><td>VSC8221 (MDIO 11)</td></tr>"
        "<tr><td>Link State</td><td><span class='%s'>%s</span></td></tr>"
        "<tr><td>Negotiated Speed</td><td>%s</td></tr>"
        "<tr><td>Duplex Mode</td><td>%s</td></tr>"
        "</table></div>"

        "<div class='card'><h2>Ethernet Traffic Counters</h2><table>"
        "<tr><td>Packets Transmitted (Tx)</td><td>%lu frames</td></tr>"
        "<tr><td>Packets Received (Rx)</td><td>%lu frames</td></tr>"
        "</table></div>"

        "<div class='card'><h2>FreeRTOS Kernel Status</h2><table>"
        "<tr><td>System Uptime</td><td>%lu seconds</td></tr>"
        "<tr><td>Active Tasks</td><td>%u tasks</td></tr>"
        "<tr><td>Free Heap Memory</td><td>%u bytes</td></tr>"
        "<tr><td>Min Ever Free Heap</td><td>%u bytes</td></tr>"
        "</table></div>"

        "</div><div class='footer'>Page auto-refreshes every 2 seconds | PolarFire SoC E51 Core</div>"
        "</body></html>",
        g_netif.name[0], g_netif.name[1],
        g_netif.hwaddr[0], g_netif.hwaddr[1], g_netif.hwaddr[2],
        g_netif.hwaddr[3], g_netif.hwaddr[4], g_netif.hwaddr[5],
        ip_str, nm_str, gw_str,
        dhcp_supplied_address(&g_netif) ? "BOUND" : (g_dhcp_requested ? "DISCOVERING..." : "DISABLED"),
        link_up ? "badge-up" : "badge-down",
        link_up ? "LINK UP" : "LINK DOWN",
        speed_str,
        fullduplex ? "Full Duplex" : "Half Duplex",
        (unsigned long)g_tx_count,
        (unsigned long)g_rx_count,
        (unsigned long)uptime_sec,
        (unsigned int)uxTaskGetNumberOfTasks(),
        (unsigned int)xPortGetFreeHeapSize(),
        (unsigned int)xPortGetMinimumEverFreeHeapSize()
    );

    file->data = g_html_buf;
    file->len = strlen(g_html_buf);
    file->index = file->len;

    return ERR_OK;
}

void fs_close(struct fs_file *file) { (void)file; }
int fs_read(struct fs_file *file, char *buffer, int count) { (void)file; (void)buffer; (void)count; return FS_READ_EOF; }
int fs_bytes_left(struct fs_file *file) { (void)file; return 0; }
