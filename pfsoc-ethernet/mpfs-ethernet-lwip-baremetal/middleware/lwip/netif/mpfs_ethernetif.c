#include <string.h>

#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_registers.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac_sw_cfg.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac_regs.h"
#include "drivers/mss/mss_ethernet_mac/mss_ethernet_mac.h"

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "netif/etharp.h"

static uint8_t g_lwip_tx_buf[1518] __attribute__((aligned(8)));

static err_t low_level_output(struct netif *netif, struct pbuf *p) {
    mss_mac_instance_t *mac = (mss_mac_instance_t *)netif->state;

    if (p->tot_len > sizeof(g_lwip_tx_buf)) {
        return ERR_MEM;
    }

    pbuf_copy_partial(p, g_lwip_tx_buf, p->tot_len, 0);

    int32_t status = MSS_MAC_send_pkt(mac, 0, g_lwip_tx_buf, p->tot_len, NULL);
    if (status == MSS_MAC_SUCCESS) {
        return ERR_OK;
    }

    return ERR_MEM;
}

err_t mss_mac_netif_init(struct netif *netif) {
    mss_mac_instance_t *mac = (mss_mac_instance_t *)netif->state;

    netif->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(netif->hwaddr, mac->mac_addr, ETH_HWADDR_LEN);
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    return ERR_OK;
}
