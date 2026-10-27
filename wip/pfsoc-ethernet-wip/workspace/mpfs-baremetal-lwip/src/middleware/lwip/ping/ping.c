#include <string.h>           /* Fixes: implicit declaration of memset */
#include "lwip/opt.h"

#if LWIP_RAW && LWIP_ICMP

#include "lwip/icmp.h"
#include "lwip/netif.h"
#include "lwip/raw.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/ip_addr.h"
#include "lwip/inet_chksum.h" /* Fixes: implicit declaration of inet_chksum */

#define PING_DELAY              1000
#define PING_DATA_SIZE          32

static struct raw_pcb *ping_pcb;
static u16_t ping_seq_num;

static void ping_send(struct raw_pcb *pcb, const ip_addr_t *addr) {
    struct pbuf *p;
    struct icmp_echo_hdr *iecho;
    size_t ping_size = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;

    p = pbuf_alloc(PBUF_RAW, (u16_t)ping_size, PBUF_RAM);
    if (!p) return;

    iecho = (struct icmp_echo_hdr *)p->payload;
    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id     = 0xAFAF;
    iecho->seqno  = lwip_htons(++ping_seq_num);

    /* Fill payload */
    memset((char *)iecho + sizeof(struct icmp_echo_hdr), 'a', PING_DATA_SIZE);

    iecho->chksum = inet_chksum(iecho, ping_size);

    raw_sendto(pcb, p, addr);
    pbuf_free(p);
}

static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    struct icmp_echo_hdr *iecho;
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);
    LWIP_UNUSED_ARG(addr);

    if (p->tot_len >= (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr))) {
        iecho = (struct icmp_echo_hdr *)((u8_t *)p->payload + PBUF_IP_HLEN);
        if (ICMPH_TYPE(iecho) == ICMP_ER) {
            printf("[PING] Echo reply received from %s\r\n", ipaddr_ntoa(addr));
            pbuf_free(p);
            return 1;
        }
    }
    return 0;
}

void ping_init(const ip_addr_t *ping_addr) {
    ping_pcb = raw_new(IP_PROTO_ICMP);
    raw_recv(ping_pcb, ping_recv, NULL);
    raw_bind(ping_pcb, IP_ADDR_ANY);
    ping_send(ping_pcb, ping_addr);
}

#endif
