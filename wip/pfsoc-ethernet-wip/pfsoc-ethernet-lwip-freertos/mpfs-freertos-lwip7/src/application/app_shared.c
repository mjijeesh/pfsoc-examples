#include <string.h>
#include "app_shared.h"
#include "drivers/mss/mss_ethernet_mac/phy.h"

mss_mac_instance_t *g_test_mac = &g_mac0;
struct netif g_netif;
SemaphoreHandle_t g_rx_sem = NULL;

uint8_t g_board_ip[4] = {192, 168, 20, 207};
volatile uint64_t g_tx_count = 0;
volatile uint64_t g_rx_count = 0;

bool g_dhcp_requested = false;
bool g_dhcp_bound_reported = false;
bool g_dhcp_fallback_active = false;
bool g_last_link_state = true;
volatile bool g_debug_monitor = false;

char g_web_log_buf[2048] = "";

void log_msg(const char *msg) {
    /* 1. Print to UART Terminal */
    PRINT_STRING(msg);

    /* 2. Append to Shared Web Terminal Buffer */
    size_t cur_len = strlen(g_web_log_buf);
    size_t msg_len = strlen(msg);

    if (cur_len + msg_len >= sizeof(g_web_log_buf) - 1) {
        /* Buffer full: Clip top half of older logs */
        size_t trim_pos = cur_len / 2;
        memmove(g_web_log_buf, g_web_log_buf + trim_pos, cur_len - trim_pos);
        g_web_log_buf[cur_len - trim_pos] = '\0';
    }
    strcat(g_web_log_buf, msg);
}
