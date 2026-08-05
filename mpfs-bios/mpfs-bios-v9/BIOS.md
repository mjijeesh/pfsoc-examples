# **Technical Specification: Persistent E51 BIOS & U54\_1 Primary Execution on Microchip PolarFire SoC (MPFS)**

## **1\. Architectural Overview**

This specification details the architecture for maintaining the **LiteX BIOS permanently running on Hart 0 (E51 Monitor Core)** while offloading bare-metal applications exclusively to the **U54 application cores (Harts 1–4)**, designated by **Hart 1 (U54\_1) as the primary lead core**.

                       \+---------------------------------------+  
                       |       PolarFire SoC (MPFS) LIM        |  
                       \+---------------------------------------+  
                       |  0x08000000 \- LiteX BIOS (Hart 0\)     | \<--- Permanent Supervisor  
                       |  0x08040000 \- Target App (Harts 1..4) | \<--- Loaded & Launched on Demand  
                       \+---------------------------------------+

\+--------------------+-------------------+---------------------------------------------------------------+  
| Core               | Execution Address | System Role                                                   |  
\+--------------------+-------------------+---------------------------------------------------------------+  
| \*\*Hart 0 (E51)\*\*   | \`0x08000000\`      | Permanent Supervisor: CLI, SFL server, System Monitor         |  
| \*\*Hart 1 (U54\_1)\*\* | \`0x08040000\`      | App Lead Core: Runs \`main\_first\_hart\_app()\`, init & app logic |  
| \*\*Hart 2 (U54\_2)\*\* | \`0x08040000\`      | App Worker Core: Synchronized with Hart 1 via IPI             |  
| \*\*Hart 3 (U54\_3)\*\* | \`0x08040000\`      | App Worker Core: Synchronized with Hart 1 via IPI             |  
| \*\*Hart 4 (U54\_4)\*\* | \`0x08040000\`      | App Worker Core: Synchronized with Hart 1 via IPI             |  
\+--------------------+-------------------+---------------------------------------------------------------+

## **2\. Technical Issues, Root-Cause Analysis & Fixes**

### **2.1 GCC Trap Instruction (ebreak / SIGTRAP at boot.c Return)**

> * **Issue**: Executing boot() on Hart 0 triggered a SIGTRAP at the closing function brace }.  
> * **Root Cause**: boot() was originally declared with \_\_attribute\_\_((noreturn)). When modified to return to the BIOS CLI loop, GCC assumed the closing brace was unreachable and emitted an explicit RISC-V ebreak instruction.  
> * **Fix**: Removed \_\_attribute\_\_((noreturn)) from boot.h and boot.c.

### **2.2 SFL Protocol Error Stream (EEEE... Output)**

> * **Issue**: After completing serialboot, the console flooded continuously with EEEE....  
> * **Root Cause**: In LiteX Serial Framing Logic (SFL), SFL\_ACK\_ERROR is represented as 'E'. When boot() returned to serialboot(), the SFL receiver loop remained active. Because litex\_term stopped sending SFL frames after the payload jump command, serialboot() timed out every 250ms, sending 'E' over UART continuously until hitting max error threshold.  
> * **Fix**: Updated SFL\_CMD\_JUMP inside serialboot() to issue return 0; immediately after calling boot(), cleanly exiting the SFL receiver loop back to main().

### **2.3 Multi-Hart Synchronization Deadlock (HLS\_BOOT\_IN\_PROGRESS)**

> * **Issue**: lim-app hung on startup with Hart 1 spinning in main\_first\_hart\_app().  
> * **Root Cause**: Microchip mpfs\_hal defaults to Hart 0 (E51) as MPFS\_HAL\_FIRST\_HART. When set to Hart 0, system\_startup.c expects Hart 0 to initialize Hart Local Storage (HLS). Since Hart 0 never entered lim-app, Hart 1 waited indefinitely for Hart 0's boot marker.  
> * **Fix**: Configured \#define MPFS\_HAL\_FIRST\_HART 1 in mss\_sw\_config.h for lim-app, making Hart 1 the designated lead core.

### **2.4 Hardware Reconfiguration & Instruction Cache Invalidation**

> * **Issue**: Executing lim-app caused Hart 0 (BIOS) to throw an instruction access fault (0x08040688).  
> * **Root Cause**: In lim-app, IMAGE\_LOADED\_BY\_BOOTLOADER was set to 0\. This enabled MPFS\_HAL\_HW\_CONFIG, causing lim-app to reconfigure L2 Cache, MPU, and PMP registers upon launch. Reconfiguring the L2 Cache controller while Hart 0 was running code out of LIM invalidated Hart 0's instruction space.  
> * **Fix**: Configured \#define IMAGE\_LOADED\_BY\_BOOTLOADER 1 and \#define MPFS\_HAL\_CLEAR\_MEMORY 0 in lim-app.

### **2.5 Serial Bus Contention**

> * **Issue**: Garbled terminal output and character loss when running both BIOS and lim-app.  
> * **Root Cause**: Hart 0 (BIOS) and Hart 1 (lim-app) both attempted to drive MMUART0 simultaneously.  
> * **Fix**: Isolated UART mapping:  
  * **Hart 0 (BIOS)**: Assigned to MMUART0 (\&g\_mss\_uart0\_lo).  
  * **Harts 1–4 (lim-app)**: Assigned to MMUART1 (\&g\_mss\_uart1\_lo).

## **3\. Implementation Artifacts**

### **3.1 BIOS Header (application/hart0/boot.h)**

C  
\#ifndef BOOT\_H\_  
\#define BOOT\_H\_

\#include \<stdint.h\>  
\#include \<stddef.h\>

// Function allows return to CLI loop (no noreturn attribute)  
void boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr);  
int serialboot(void);

\#endif /\* BOOT\_H\_ \*/

### **3.2 BIOS Non-Blocking Handoff (application/hart0/boot.c)**

C  
\#include \<stdio.h\>  
\#include \<stdint.h\>  
\#include \<string.h\>

\#include "mpfs\_hal/mss\_hal.h"  
\#include "sfl.h"  
\#include "crc.h"  
\#include "boot.h"  
\#include "uart.h"

volatile uint64\_t g\_app\_jump\_addr \= 0;

void boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr)  
{  
    (void)r1; (void)r2; (void)r3;

    printf("Launching target application on U54 harts at 0x%016lx...\\n", addr);  
    printf("E51 Monitor Core remains active in BIOS.\\n\\n");  
    uart\_sync();

    // 1\. Publish jump address for U54 harts (1..4)  
    g\_app\_jump\_addr \= addr;  
    asm volatile("fence" ::: "memory");

    // 2\. Raise IPIs strictly to U54 cores  
    for (uint32\_t hart\_id \= 1; hart\_id \<= 4; hart\_id++) {  
        raise\_soft\_interrupt(hart\_id);  
    }

    // 3\. Delay allowing U54 cores to unblock from wfi and fetch jump address  
    for (volatile int i \= 0; i \< 200000; i++);

    asm volatile("fence.i" ::: "memory");

    // Hart 0 DOES NOT call entry(). Returns to serialboot() \-\> CLI loop.  
}

int serialboot(void)  
{  
    // ... \[SFL Frame Receiving Logic\] ...

        case SFL\_CMD\_JUMP: {  
            uint32\_t jump\_addr;

            if (frame.payload\_length \< 4\) {  
                uart\_write(SFL\_ACK\_ERROR);  
                if (serialboot\_fail(\&failures)) return 1;  
                break;  
            }

            failures \= 0;  
            uart\_write(SFL\_ACK\_SUCCESS);  
            jump\_addr \= get\_uint32(\&frame.payload\[0\]);

            // Execute boot handoff to U54 cores  
            boot(0, 0, 0, (unsigned long)jump\_addr);

            // Exit SFL loop cleanly back to litex\> prompt  
            return 0;  
        }  
    // ...  
}  
\`\`\`\[cite: 7\]

\---

\#\#\# 3.3 BIOS Secondary Hart Wakeup (\`application/hart1/u54\_1.c\`)  
\*(Identical implementation applied to \`u54\_2.c\`\[cite: 10\], \`u54\_3.c\`\[cite: 9\], and \`u54\_4.c\`\[cite: 8\] in the BIOS project)\*

\`\`\`c  
\#include "mpfs\_hal/mss\_hal.h"

extern volatile uint64\_t g\_app\_jump\_addr;

typedef void (\*entry\_func\_t)(unsigned long r1, unsigned long r2, unsigned long r3);

void u54\_1(void)   
{  
    clear\_soft\_interrupt();  
    set\_csr(mie, MIP\_MSIP);

    while (1) {  
        // Wait for IPI signal from Hart 0  
        \_\_asm\_\_ volatile ("wfi");

        // Unblock check for published application address  
        if (g\_app\_jump\_addr \!= 0\) {  
            clear\_soft\_interrupt();  
            entry\_func\_t entry \= (entry\_func\_t)(uintptr\_t)g\_app\_jump\_addr;

            asm volatile("fence" ::: "memory");  
            asm volatile("fence.i" ::: "memory");

            // Jump directly to application entry (\_start)  
            entry(0, 0, 0);  
        }  
    }  
}

void Software\_h1\_IRQHandler(void)  
{  
    clear\_soft\_interrupt();  
}  
\`\`\`\[cite: 11\]

\---

\#\#\# 3.4 Target App Software Config (\`mss\_sw\_config.h\`)  
\`\`\`c  
\#ifndef MSS\_SW\_CONFIG\_H\_  
\#define MSS\_SW\_CONFIG\_H\_

// Configure Hart 1 (U54\_1) as the lead core for system\_startup.c  
\#ifndef MPFS\_HAL\_FIRST\_HART  
\#define MPFS\_HAL\_FIRST\_HART    1  
\#endif

\#ifndef MPFS\_HAL\_LAST\_HART  
\#define MPFS\_HAL\_LAST\_HART     4  
\#endif

// Prevent HAL hardware reconfiguration that disrupts E51  
\#define IMAGE\_LOADED\_BY\_BOOTLOADER 1

// Prevent HAL memory zeroing in RAM  
\#ifndef MPFS\_HAL\_CLEAR\_MEMORY  
\#define MPFS\_HAL\_CLEAR\_MEMORY  0  
\#endif

\#endif /\* MSS\_SW\_CONFIG\_H\_ \*/  
\`\`\`\[cite: 12\]

\---

\#\#\# 3.5 Target App Build Configuration (\`Makefile\`)  
\`\`\`makefile  
else ifeq ($(MEM\_TARGET),lim-app)  
    BUILD\_DIR     := build/lim-app  
    LINKER\_SCRIPT := $(BOARD\_DIR)/platform\_config/lim-release/linker/mpfs-lim-app.ld  
    CONFIG\_DIR    := lim-release  
    CFLAGS        \+= \-DIMAGE\_LOADED\_BY\_BOOTLOADER=1  
    CFLAGS        \+= \-DMPFS\_HAL\_CLEAR\_MEMORY=0  
    CFLAGS        \+= \-DMPFS\_HAL\_FIRST\_HART=1 \-DMPFS\_HAL\_LAST\_HART=4

\# Route stdio output of target application to MMUART1  
CFLAGS \+= \-DMICROCHIP\_STDIO\_THRU\_MMUARTX=\\\&g\_mss\_uart1\_lo  
\`\`\`\[cite: 13\]

\---

\#\#\# 3.6 Target App Lead Core (\`application/hart1/u54\_1.c\`)  
\`\`\`c  
\#include \<stdio.h\>  
\#include "mpfs\_hal/mss\_hal.h"

void u54\_1(void)   
{  
    // Clear boot IPI sent by Hart 0  
    clear\_soft\_interrupt();

    printf("\\n\[U54\_1\] Target application started successfully.\\n");  
    printf("\[U54\_1\] Lead core active on Hart 1.\\n");

    // Primary application execution loop  
    while (1) {  
        // Application logic  
    }  
}

## **4\. Execution & Verification Protocol**

### **1\. Launch Sequence**

Bash  
\# Terminal 1: Run litex\_term  
litex\_term \--serialboot \--kernel build/lim-app/pfsoc\_app.bin \--base-address 0x08040000 /dev/ttyUSB0

Inside the BIOS terminal:

Plaintext  
litex\> serialboot  
Booting from serial interface (SFL)...  
\[LITEX-TERM\] Uploading build/lim-app/pfsoc\_app.bin to 0x08040000...  
\[LITEX-TERM\] Upload complete.  
\[LITEX-TERM\] Done.

Launching target application on U54 harts at 0x0000000008040000...  
E51 Monitor Core remains active in BIOS.

litex\> 

### **2\. GDB Verification Procedure**

Attach GDB to OpenOCD to verify persistent execution of BIOS on Thread 1 and payload execution on Threads 2–5:

Code snippet  
(gdb) thread apply all info reg pc

**Verified Output:**

Plaintext  
Thread 5 (Thread 5 (Name: mpfs.hart4\_u54\_4, state: debug-request)):  
pc             0x804205e    0x804205e \<u54\_4\>

Thread 4 (Thread 4 (Name: mpfs.hart3\_u54\_3, state: debug-request)):  
pc             0x8041fe4    0x8041fe4 \<u54\_3\>

Thread 3 (Thread 3 (Name: mpfs.hart2\_u54\_2, state: debug-request)):  
pc             0x8041f6a    0x8041f6a \<u54\_2\>

Thread 2 (Thread 2 (Name: mpfs.hart1\_u54\_1, state: debug-request)):  
pc             0x8042200    0x8042200 \<u54\_1\>

Thread 1 (Thread 1 (Name: mpfs.hart0\_e51, state: debug-request)):  
pc             0x80012c4    0x80012c4 \<main+112\>

> * **Thread 1 (E51)**: Active at 0x080012c4 inside the LiteX BIOS CLI loop.  
> * **Threads 2–5 (U54\_1..4)**: Active at 0x0804xxxx running the offloaded application.