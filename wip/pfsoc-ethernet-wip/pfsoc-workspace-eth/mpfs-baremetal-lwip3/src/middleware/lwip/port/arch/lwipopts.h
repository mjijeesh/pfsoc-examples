#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#include <stdint.h>
#include <sys/types.h>

#define LWIP_TYPEDEF_SSIZE_T    1
#define NO_SYS                  1   /* Bare-metal mode (No RTOS) */
#define MEM_ALIGNMENT           8   /* 64-bit alignment for RISC-V */

#define ETH_PAD_SIZE            0
#define LWIP_UNALIGNED_OK       0   /* Prevents RISC-V strict-alignment traps */


/* Core Protocols */
#define LWIP_ARP                1
#define LWIP_ETHERNET           1
#define LWIP_ICMP               1
#define LWIP_RAW                1
#define LWIP_IP4                1
#define LWIP_UDP                1
#define LWIP_TCP                1



/* Memory Allocation */
#define MEM_SIZE                (16 * 1024)
#define PBUF_POOL_SIZE          16
#define PBUF_POOL_BUFSIZE       1536

/* Disable OS / Socket APIs & HTTPD */
#define LWIP_SOCKET             0
#define LWIP_NETCONN            0
#define LWIP_TIMEVAL_PRIVATE    0



#define LWIP_HTTPD               1
#define LWIP_HTTPD_SSI           1
#define LWIP_HTTPD_CGI           1


/* Software Checksum Generation */
#define CHECKSUM_GEN_IP         1
#define CHECKSUM_GEN_UDP        1
#define CHECKSUM_GEN_TCP        1
#define CHECKSUM_GEN_ICMP       1
#define CHECKSUM_CHECK_IP       1
#define CHECKSUM_CHECK_UDP      1
#define CHECKSUM_CHECK_TCP      1
#define CHECKSUM_CHECK_ICMP     1

/* Enable DHCP Client in lwIP Stack */
#define LWIP_DHCP               1
#define LWIP_UDP                1

#endif /* LWIP_LWIPOPTS_H */
