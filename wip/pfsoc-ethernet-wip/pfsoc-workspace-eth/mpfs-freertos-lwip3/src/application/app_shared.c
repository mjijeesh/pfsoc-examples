#include "app_shared.h"
#include "drivers/mss/mss_ethernet_mac/phy.h"

/* Central Ownership of Global State Variables */
mss_mac_instance_t *g_test_mac = &g_mac0;
struct netif g_netif;
SemaphoreHandle_t g_rx_sem = NULL;

uint8_t g_board_ip[4] = {192, 168, 20, 207};
volatile uint64_t g_tx_count = 0;
volatile uint64_t g_rx_count = 0;
bool g_dhcp_requested = false;
bool g_dhcp_bound_reported = false;
bool g_last_link_state = true;
volatile bool g_debug_monitor = false;
