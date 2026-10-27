#ifndef APP_SHARED_H
#define APP_SHARED_H

#include <stdint.h>
#include <stdbool.h>

#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"

/* Mandatory MSS Ethernet MAC Driver Prerequisites (Must precede mss_ethernet_mac.h) */
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_registers.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac_sw_cfg.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac_regs.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "lwip/netif.h"

#define DEMO_UART       &g_mss_uart0_lo
#define PRINT_STRING(x) MSS_UART_polled_tx_string(DEMO_UART, (const uint8_t *)x);

/* Shared Network and Driver Declarations */
extern mss_mac_instance_t *g_test_mac;
extern struct netif g_netif;
extern SemaphoreHandle_t g_rx_sem;
extern uint8_t g_board_ip[4];

extern volatile uint64_t g_tx_count;
extern volatile uint64_t g_rx_count;
extern bool g_dhcp_requested;
extern bool g_dhcp_bound_reported;
extern bool g_last_link_state;
extern volatile bool g_debug_monitor;

#endif /* APP_SHARED_H */
