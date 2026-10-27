#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#include <stdint.h>
#include <sys/types.h>

/* Include FreeRTOS headers to resolve configMAX_PRIORITIES */
#include "FreeRTOS.h"


#define LWIP_TYPEDEF_SSIZE_T    1
#define NO_SYS                  0   /* Enable FreeRTOS OS Mode */
#define SYS_LIGHTWEIGHT_PROT    1   /* Thread safety for memory pools */
#define MEM_ALIGNMENT           8   /* 64-bit alignment for RISC-V */

#define ETH_PAD_SIZE            0
#define LWIP_UNALIGNED_OK       0   /* Prevents RISC-V strict-alignment traps */

/* FreeRTOS Core Thread & Mailbox Settings */
#define TCPIP_THREAD_NAME       "tcpip_thread"
#define TCPIP_THREAD_STACKSIZE  2048
#define TCPIP_THREAD_PRIO       (configMAX_PRIORITIES - 2)
#define TCPIP_MBOX_SIZE         32

#define DEFAULT_RAW_RECVMBOX_SIZE  16
#define DEFAULT_UDP_RECVMBOX_SIZE  16
#define DEFAULT_TCP_RECVMBOX_SIZE  16
#define DEFAULT_ACCEPTMBOX_SIZE    16

/* Core Protocols */
#define LWIP_ARP                1
#define LWIP_ETHERNET           1
#define LWIP_ICMP               1
#define LWIP_RAW                1
#define LWIP_IP4                1
#define LWIP_UDP                1
#define LWIP_TCP                1

/* Memory Allocations (Expanded for OS Queues) */
#define MEM_SIZE                (32 * 1024)
#define PBUF_POOL_SIZE          32
#define PBUF_POOL_BUFSIZE       1536

/* Enable OS Socket & Netconn APIs */
#define LWIP_NETCONN            1
#define LWIP_SOCKET             1
#define LWIP_TIMEVAL_PRIVATE    0

/* Web Server Services */
#define LWIP_HTTPD              1
#define LWIP_HTTPD_SSI          1
#define LWIP_HTTPD_CGI          1

/* Software Checksum Generation */
#define CHECKSUM_GEN_IP         1
#define CHECKSUM_GEN_UDP        1
#define CHECKSUM_GEN_TCP        1
#define CHECKSUM_GEN_ICMP       1
#define CHECKSUM_CHECK_IP       1
#define CHECKSUM_CHECK_UDP      1
#define CHECKSUM_CHECK_TCP      1
#define CHECKSUM_CHECK_ICMP     1

/* Enable DHCP Client */
#define LWIP_DHCP               1

#endif /* LWIP_LWIPOPTS_H */
