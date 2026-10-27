#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#include <stdint.h>  /* Fixes: unknown type name 'uint32_t' */

#define NO_SYS                  1   /* Bare-metal mode (No RTOS) */
#define MEM_ALIGNMENT           8   /* 64-bit alignment for RISC-V */

/* Disable OS/Socket features in bare-metal */
#define LWIP_SOCKET             0
#define LWIP_NETCONN            0

/* Use Newlib's built-in struct timeval */
#define LWIP_TIMEVAL_PRIVATE    0

/* Object-like macro so #ifndef sys_now in lwip/sys.h recognizes it */
 extern volatile uint32_t g_tick_counter;
//#define sys_now                 g_tick_counter


#define LWIP_RAW                1
#define LWIP_ARP                1
#define LWIP_ICMP               1
#define LWIP_DHCP               1
#define LWIP_TCP                1
#define LWIP_UDP                1

 /* Enable HTTPD App */
 #define LWIP_HTTPD              1
 #define HTTPD_USE_CUSTOM_FSI    1

 /* Disable Application Layered TCP (ALTCP) for standard HTTPD */
 #define LWIP_ALTCP              0
 #define LWIP_ALTCP_TCP          0

 /* CORRECT */
 //#define HTTPD_FSDATA_FILE "fsdata.c"

 /* INCORRECT - DO NOT DO THIS */
 /* #define HTTPD_FSDATA_FILE */


#endif
