/*******************************************************************************
 * Dynamic HTTP Web Dashboard Module
 * Features:
 *  - FreeRTOS Task List, Execution State & Stack High-Water Memory Usage
 *  - Real-time Tx/Rx Ethernet Packet Counters
 *  - Interactive Traffic Monitor Toggle Switch (Virtual URI Path Handler)
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "app_shared.h"
#include "lwip/apps/fs.h"
#include "lwip/dhcp.h"

#include "FreeRTOS.h"
#include "task.h"

static char g_html_buf[4096];

static const char *get_task_state_str(eTaskState state) {
    switch (state) {
        case eRunning:   return "<span style='color:#4ade80;'>RUNNING</span>";
        case eReady:     return "<span style='color:#38bdf8;'>READY</span>";
        case eBlocked:   return "<span style='color:#facc15;'>BLOCKED</span>";
        case eSuspended: return "<span style='color:#f87171;'>SUSPENDED</span>";
        case eDeleted:   return "DELETED";
        default:         return "UNKNOWN";
    }
}

err_t fs_open(struct fs_file *file, const char *name) {
    memset(file, 0, sizeof(struct fs_file));

    /* Parse explicit URI path actions to toggle real-time CLI traffic monitor */
    if (name != NULL) {
        if (strstr(name, "monitor_on") != NULL) {
            g_debug_monitor = true;
        } else if (strstr(name, "monitor_off") != NULL) {
            g_debug_monitor = false;
        }
    }

    /* Query PHY Hardware Link & Speed Status */
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

    /* Build HTML Header, Dashboard Cards, and CSS Styling */
    int len = snprintf(g_html_buf, sizeof(g_html_buf),
        "<!DOCTYPE html><html><head>"
        "<meta http-equiv='refresh' content='2;url=/'>"
        "<title>PolarFire SoC Dashboard</title>"
        "<style>"
        "body{font-family:Segoe UI,Arial,sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:20px;}"
        "h1{color:#38bdf8;border-bottom:2px solid #334155;padding-bottom:10px;margin-top:0;}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:15px;}"
        ".card{background:#1e293b;border-radius:8px;padding:18px;box-shadow:0 4px 6px -1px rgba(0,0,0,0.3);}"
        ".card h2{margin-top:0;color:#f1f5f9;font-size:18px;border-bottom:1px solid #334155;padding-bottom:8px;}"
        "table{width:100%%;border-collapse:collapse;margin-top:5px;}"
        "th{text-align:left;padding:6px 0;color:#94a3b8;font-size:12px;border-bottom:1px solid #334155;}"
        "td{padding:8px 0;border-bottom:1px solid #334155;font-size:13px;}"
        "td:last-child{text-align:right;font-weight:600;color:#38bdf8;}"
        ".badge-up{background:#166534;color:#4ade80;padding:3px 8px;border-radius:4px;font-size:12px;}"
        ".badge-down{background:#991b1b;color:#f87171;padding:3px 8px;border-radius:4px;font-size:12px;}"
        ".btn{padding:5px 12px;border-radius:4px;text-decoration:none;font-size:12px;font-weight:bold;display:inline-block;margin-left:4px;}"
        ".btn-active{background:#0284c7;color:#ffffff;}"
        ".btn-inactive{background:#334155;color:#94a3b8;}"
        ".footer{margin-top:20px;font-size:12px;color:#64748b;text-align:center;}"
        "</style></head><body>"
        "<h1>PolarFire SoC Real-Time OS & Network Node</h1>"
        "<div class='grid'>"

        /* Card 1: Network & MAC Details */
        "<div class='card'><h2>MAC & Network Layer</h2><table>"
        "<tr><td>Interface</td><td>%c%c</td></tr>"
        "<tr><td>MAC Address</td><td>%02X:%02X:%02X:%02X:%02X:%02X</td></tr>"
        "<tr><td>IPv4 Address</td><td>%s</td></tr>"
        "<tr><td>Subnet Mask</td><td>%s</td></tr>"
        "<tr><td>Gateway</td><td>%s</td></tr>"
        "<tr><td>DHCP Status</td><td>%s</td></tr>"
        "</table></div>"

        /* Card 2: Hardware PHY Transceiver */
        "<div class='card'><h2>PHY Hardware Status</h2><table>"
        "<tr><td>Transceiver</td><td>VSC8221 (MDIO 11)</td></tr>"
        "<tr><td>Link State</td><td><span class='%s'>%s</span></td></tr>"
        "<tr><td>Negotiated Speed</td><td>%s</td></tr>"
        "<tr><td>Duplex Mode</td><td>%s</td></tr>"
        "</table></div>"

        /* Card 3: Network Traffic & Traffic Inspector Control */
        "<div class='card'><h2>Traffic & Live Inspector</h2><table>"
        "<tr><td>Packets Transmitted (Tx)</td><td>%lu frames</td></tr>"
        "<tr><td>Packets Received (Rx)</td><td>%lu frames</td></tr>"
        "<tr><td>CLI Packet Inspector</td><td>%s</td></tr>"
        "<tr><td>Toggle Inspector</td><td>"
        "<a href='/monitor_on.html' class='btn %s'>ON</a>"
        "<a href='/monitor_off.html' class='btn %s'>OFF</a>"
        "</td></tr>"
        "</table></div>"

        /* Card 4: FreeRTOS Global Memory */
        "<div class='card'><h2>FreeRTOS Heap Overview</h2><table>"
        "<tr><td>System Uptime</td><td>%lu seconds</td></tr>"
        "<tr><td>Total Active Tasks</td><td>%u tasks</td></tr>"
        "<tr><td>Current Free Heap</td><td>%u bytes</td></tr>"
        "<tr><td>Min Ever Free Heap</td><td>%u bytes</td></tr>"
        "</table></div>",

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
        g_debug_monitor ? "<span style='color:#4ade80;'>ACTIVE (ON)</span>" : "<span style='color:#f87171;'>INACTIVE (OFF)</span>",
        g_debug_monitor ? "btn-active" : "btn-inactive",
        !g_debug_monitor ? "btn-active" : "btn-inactive",
        (unsigned long)uptime_sec,
        (unsigned int)uxTaskGetNumberOfTasks(),
        (unsigned int)xPortGetFreeHeapSize(),
        (unsigned int)xPortGetMinimumEverFreeHeapSize()
    );

    /* Card 5: FreeRTOS Task Breakdown Table */
    len += snprintf(g_html_buf + len, sizeof(g_html_buf) - len,
        "<div class='card' style='grid-column:1/-1;'><h2>FreeRTOS Task Breakdown & Memory High-Water Mark</h2>"
        "<table><tr><th>Task Name</th><th>State</th><th>Priority</th><th>Task ID</th><th>Remaining Stack Room</th></tr>"
    );

    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));

    if (task_array != NULL) {
        task_count = uxTaskGetSystemState(task_array, task_count, NULL);
        for (UBaseType_t i = 0; i < task_count; i++) {
            uint32_t stack_free_bytes = (uint32_t)(task_array[i].usStackHighWaterMark * sizeof(StackType_t));

            len += snprintf(g_html_buf + len, sizeof(g_html_buf) - len,
                "<tr><td>%s</td><td>%s</td><td>%u</td><td>#%u</td><td>%u bytes free</td></tr>",
                task_array[i].pcTaskName,
                get_task_state_str(task_array[i].eCurrentState),
                (unsigned int)task_array[i].uxCurrentPriority,
                (unsigned int)task_array[i].xTaskNumber,
                (unsigned int)stack_free_bytes
            );
        }
        vPortFree(task_array);
    } else {
        len += snprintf(g_html_buf + len, sizeof(g_html_buf) - len,
            "<tr><td colspan='5' style='color:#f87171;'>Unable to allocate memory for task stats.</td></tr>"
        );
    }

    len += snprintf(g_html_buf + len, sizeof(g_html_buf) - len,
        "</table></div>"
        "</div><div class='footer'>Auto-refreshes every 2s | PolarFire SoC E51 Core | FreeRTOS + lwIP Engine</div>"
        "</body></html>"
    );

    file->data = g_html_buf;
    file->len = strlen(g_html_buf);
    file->index = file->len;
    return ERR_OK;
}

void fs_close(struct fs_file *file) { (void)file; }
int fs_read(struct fs_file *file, char *buffer, int count) { (void)file; (void)buffer; (void)count; return FS_READ_EOF; }
int fs_bytes_left(struct fs_file *file) { (void)file; return 0; }
