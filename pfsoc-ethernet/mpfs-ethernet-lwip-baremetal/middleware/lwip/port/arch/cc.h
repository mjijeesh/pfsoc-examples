#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <sys/types.h>
#include <errno.h>  /* Include standard POSIX error codes */


#ifndef SSIZE_MAX
  #ifdef __SSIZE_MAX__
    #define SSIZE_MAX __SSIZE_MAX__
  #elif defined(LONG_MAX)
    #define SSIZE_MAX LONG_MAX
  #else
    #define SSIZE_MAX 0x7FFFFFFF
  #endif
#endif


#define LWIP_NO_UNISTD_H 1

#ifdef BYTE_ORDER
#undef BYTE_ORDER
#endif
#define BYTE_ORDER LITTLE_ENDIAN

typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;

typedef uint32_t  sys_prot_t;

#define LWIP_PLATFORM_DIAG(x)   do { printf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { printf("Assert: %s\n", x); } while(0)

#endif /* LWIP_ARCH_CC_H */
