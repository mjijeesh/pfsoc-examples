/*******************************************************************************
 * Network Subsystem: Ethernet MAC Driver & Network Tasks
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "net_tasks.h"
#include "app_shared.h"
#include "uart_cli.h"

#include "drivers/mss/mss_ethernet_mac/phy.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/apps/httpd.h"

#define PACKET_MAX      1518U
#define TX_SLOT_SIZE    1536U
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

static int send_ethernet_frame(const uint8_t *buf, uint32_t len) {
    if (len > PACKET_MAX) len = PACKET_MAX;
    if (g_test_mac->queue[0].nb_available_tx_desc == 0) return MSS_MAC_FAILED;

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
    netif->name[0] = 'e'; netif->name[1] = '0';
    netif->output = etharp_output;
    netif->linkoutput = lwip_mac_output;
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, g_test_mac->mac_addr, 6);
    return ERR_OK;
}

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
            if (g_rx_sem != NULL) xSemaphoreGiveFromISR(g_rx_sem, &xHigherPriorityTaskWoken);
        }
    }
    MSS_MAC_receive_pkt((mss_mac_instance_t *)this_mac, 0, p_rx_packet, 0, 1);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void packet_tx_complete_handler(void *this_mac, uint32_t queue_no, mss_mac_tx_desc_t *cdesc, void *caller_info) {
    (void)caller_info; (void)cdesc; (void)this_mac; (void)queue_no;
    g_tx_count++;
}

void low_level_init(void) {
    PRINT_STRING("[DRIVER] Initializing MSS MAC configuration structures...\r\n");
    MSS_MAC_cfg_struct_def_init(&g_mac_config);

    g_test_mac = &g_mac0;
    g_mac_config.speed_duplex_select = MSS_MAC_ANEG_ALL_SPEEDS;
    g_mac_config.mac_addr[0] = 0x00; g_mac_config.mac_addr[1] = 0xFC; g_mac_config.mac_addr[2] = 0x00;
    g_mac_config.mac_addr[3] = 0x12; g_mac_config.mac_addr[4] = 0x34; g_mac_config.mac_addr[5] = 0x58;
    g_mac_config.tsu_clock_select = 1U;
    g_mac_config.phy_addr = PHY_VSC8221_MDIO_ADDR;
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
        MSS_MAC_receive_pkt(g_test_mac, 0, g_mac_rx_buffer[count], 0, (count != (MSS_MAC_RX_RING_SIZE - 1)) ? 0 : -1);
    }
    PRINT_STRING("[DRIVER] Low-level GEM0 DMA hardware setup complete.\r\n");
}

void tcpip_init_done_cb(void *arg) {
    (void)arg;
    ip4_addr_t ipaddr, netmask, gw;

    PRINT_STRING("[STACK] lwIP tcpip_thread initialized. Setting up netif...\r\n");

    IP4_ADDR(&ipaddr, g_board_ip[0], g_board_ip[1], g_board_ip[2], g_board_ip[3]);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, g_board_ip[0], g_board_ip[1], g_board_ip[2], 1);

    netif_add(&g_netif, &ipaddr, &netmask, &gw, g_test_mac, lwip_netif_init, tcpip_input);
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

void eth_rx_task(void *pvParameters) {
    (void)pvParameters;
    char dbg_buf[128];

    while (1) {
        if (xSemaphoreTake(g_rx_sem, portMAX_DELAY) == pdTRUE) {
            while (g_rx_q_head != g_rx_q_tail) {
                uint32_t current_tail = g_rx_q_tail;
                uint32_t len = g_rx_raw_queue[current_tail].len;
                uint8_t *data = g_rx_raw_queue[current_tail].data;

                if (g_debug_monitor && len >= 14) {
                    uint16_t ethertype = (data[12] << 8) | data[13];
                    if (ethertype == 0x0806 && len >= 28) {
                        uint16_t op = (data[20] << 8) | data[21];
                        if (op == 1 && data[38] == g_board_ip[0] && data[39] == g_board_ip[1]) {
                            sprintf(dbg_buf, "\r\n[DEBUG] RX ARP Request for %u.%u.%u.%u\r\neth-cli> ", data[38], data[39], data[40], data[41]);
                            PRINT_STRING(dbg_buf);
                        }
                    }
                }

                struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
                if (p != NULL) {
                    pbuf_take(p, data, (u16_t)len);
                    if (g_netif.input(p, &g_netif) != ERR_OK) pbuf_free(p);
                }
                __sync_synchronize();
                g_rx_q_tail = (current_tail + 1) % RX_QUEUE_SIZE;
            }
        }
    }
}

void phy_monitor_task(void *pvParameters) {
    (void)pvParameters;
    char dbg_buf[128];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        mss_mac_speed_t speed; uint8_t fullduplex;
        uint8_t link_up = MSS_MAC_get_link_status(g_test_mac, &speed, &fullduplex);

        if (link_up && !g_last_link_state) {
            g_last_link_state = true; netif_set_link_up(&g_netif);
            PRINT_STRING("\r\n[PHY EVENT] Link RESTORED.\r\neth-cli> ");
        } else if (!link_up && g_last_link_state) {
            g_last_link_state = false; netif_set_link_down(&g_netif);
            PRINT_STRING("\r\n[PHY WARNING] Link LOST.\r\neth-cli> ");
        }

        if (g_dhcp_requested && dhcp_supplied_address(&g_netif) && !g_dhcp_bound_reported) {
            g_dhcp_bound_reported = true;
            sprintf(dbg_buf, "\r\n[DHCP SUCCESS] IP: %s\r\neth-cli> ", ipaddr_ntoa(&g_netif.ip_addr));
            PRINT_STRING(dbg_buf);
        }
    }
}
