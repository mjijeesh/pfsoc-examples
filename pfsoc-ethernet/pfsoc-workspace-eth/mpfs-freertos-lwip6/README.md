lwip + freertos Demo.

Discovery Kit Tested
v6 :  using thr Makefile project sam as the v5 
V5:  Added dynamic CLI output in the web dash board. on/off switch i nthe web dash for monitor
v4 : added monitor on/off on the webdash baord, increased the stack size of freertos taks, added stackoverflow function for errro reprotign 
v3: added dynamic web page with status information
v2: Added detailed debug messages and help menu
v1 : Basic

Here is the complete technical documentation detailing the issues encountered, root causes identified, and solutions implemented during the porting and feature enhancement of the PolarFire SoC (E51 Core) FreeRTOS + lwIP network engine.
1. Kernel Architecture & RISC-V Interrupt Routing
PLIC Trap Loop (application_interrupt_handler Freeze)
Root Cause: FreeRTOS RISC-V port assembly (portASM.S) defaulted external machine interrupts to an unhandled dummy trap loop, hanging the CPU when hardware interrupts occurred.
Fix: Defined #define portasmHANDLE_INTERRUPT handle_m_ext_interrupt in FreeRTOSConfig.h and implemented freertos_risc_v_application_interrupt_handler() in main.c to route PLIC events directly to the Microchip HAL dispatcher.
UART Interrupt Storm Lockup
Root Cause: Enabling MMUART0 interrupts at the PLIC without an active ISR servicing the UART Receiver Buffer Register (RBR) caused continuous interrupt re-triggering, starving OS tasks.
Fix: Kept MMUART0_E51_INT disabled in the PLIC and implemented non-blocking UART character polling (MSS_UART_get_rx) inside cli_task.
2. Driver Initialization & Hardware HAL
MSS Ethernet MAC Header Dependency Chain
Root Cause: Compilation failed with MSS_MAC_QUEUE_COUNT and MAC_TypeDef undeclared errors when mss_ethernet_mac.h was included independently.
Fix: Updated header inclusion order in app_shared.h to explicitly include mss_ethernet_registers.h, mss_ethernet_mac_sw_cfg.h, and mss_ethernet_mac_regs.h before mss_ethernet_mac.h.
Invalid PLIC Symbol References
Root Cause: Linking errors occurred when attempting to use non-existent HAL symbols (MAC0_INT_E51_INT, PLIC_SetHandler, and MAC0_u54_IRQHandler).
Fix: Standardized interrupt setup in e51() using valid MPFS HAL definitions: PLIC_SetPriority(MAC0_INT_U54_INT, 2) and PLIC_EnableIRQ(MAC0_INT_U54_INT).
3. Network Stack & Auto-DHCP Fallback
Auto-DHCP Discovery with Static IP Fallback
Requirement: System must attempt dynamic IP assignment on boot without locking network access if no DHCP server is detected.
Fix: Integrated an automated 10-second timer inside phy_monitor_task. If DHCP binding fails after 10 seconds, the stack stops DHCP and assigns static IP 192.168.20.207.
Unified Console & Web Logging Stream
Requirement: Terminal messages needed to display simultaneously in the UART console and the web interface.
Fix: Built log_msg() in app_shared.c to forward text both to MSS_UART_polled_tx_string and a shared 2 KB circular web log buffer (g_web_log_buf).
4. RV64GC Stack Memory & Dynamic Web Dashboard
Task Stack Overflow (vApplicationStackOverflowHook Crash)
Root Cause: Formatting dynamic HTML strings (snprintf) on 64-bit RISC-V (RV64GC) doubled stack frame memory requirements, causing tcpip_thread and application tasks to exceed default stack boundaries.
Fix: Increased TCPIP_THREAD_STACKSIZE in lwipopts.h to 4096, and doubled stack allocations in main.c (ETH_RX to 2048, CLI_TASK to 2048, PHY_MON to 1024 words).
HTML Dashboard Truncation (Missing Task Table Rows)
Root Cause: The HTML output buffer (g_html_buf) size of 5120 bytes was insufficient to hold CSS styles, the live console log window, and all 6 FreeRTOS task stats, cutting off string formatting mid-page.
Fix: Expanded g_html_buf to 12 KB (12288 bytes) in web_dashboard.c and updated TaskStatus_t member access to eCurrentState.
Non-Responsive HTTP Monitor Toggle
Root Cause: The lwIP HTTP server stripped standard URL query parameters (/?monitor=on) before passing requested filenames to fs_open().
Fix: Replaced query parameters with explicit virtual URI paths (/monitor_on.html and /monitor_off.html), enabling fs_open() to intercept path strings and toggle g_debug_monitor.




1. Memory & Linker Script Configurations
Missing FreeRTOS ISR Stack Pointer (__freertos_irq_stack_top)
Problem: The bare-metal linker script (mpfs-lim.ld) lacked the FreeRTOS-specific ISR stack symbol required by portASM.S.
Fix: Added PROVIDE(__freertos_irq_stack_top = .); inside the .stack section of mpfs-lim.ld.
Duplicate OS Symbol Linker Redefinitions
Problem: Functions sys_now(), sys_arch_protect(), and sys_arch_unprotect() were defined in both sys_arch.c (lwIP FreeRTOS port) and main.c.
Fix: Removed the duplicate implementations from main.c, leaving sys_arch.c as the sole provider.
Missing FreeRTOS Application Hook Functions
Problem: FreeRTOSConfig.h enabled configASSERT, configCHECK_FOR_STACK_OVERFLOW, and configUSE_MALLOC_FAILED_HOOK, causing linker errors for missing callbacks.
Fix: Implemented vAssertCalled(), vApplicationMallocFailedHook(), and vApplicationStackOverflowHook() inside main.c.
2. FreeRTOS Kernel & Hardware Prescaler Updates
Undeclared TICK_COUNT_PRESCALER Symbol
Problem: port.c attempted to use TICK_COUNT_PRESCALER because RENODE_SIMULATION was not set to 1 in the new build environment.
Fix: Updated the timer increment calculation in port.c to use direct hardware clock division: (size_t)((configCPU_CLOCK_HZ) / (configTICK_RATE_HZ)).
FreeRTOS Source/Header Version Collision
Problem: Core kernel .c files (stream_buffer.c, event_groups.c) were from an older FreeRTOS build, conflicting with parameter definitions in newer header files.
Fix: Replaced all kernel .c and .h files with matching source files from FreeRTOS V202212.00.
Trap Vector CSR (mtvec) Initialization
Problem: Upon calling vTaskStartScheduler(), the CPU defaulted to bare-metal trap handling instead of context-switching.
Fix: Assigned freertos_risc_v_trap_handler to mtvec via write_csr(mtvec, (uint64_t)freertos_risc_v_trap_handler) prior to scheduler launch.
3. lwIP OS Abstraction & Stack Porting
Bare-Metal (NO_SYS = 1) to OS (NO_SYS = 0) Architecture Shift
Problem: Bare-metal lwIP relied on continuous polling loops without thread synchronization primitives.
Fix: Reconfigured lwipopts.h with NO_SYS = 0 and SYS_LIGHTWEIGHT_PROT = 1. Created dedicated FreeRTOS tasks (eth_rx_task, phy_monitor_task, cli_task) and used binary semaphores for deferred RX packet processing.
Toolchain SSIZE_MAX Macro Expansion Failure
Problem: RISC-V GCC toolchains do not predefine __SSIZE_MAX__, causing compilation errors in api_lib.c.
Fix: Added a fallback definition in cc.h checking for __SSIZE_MAX__, LONG_MAX, or defaulting to 0x7FFFFFFF.
Missing POSIX Network Error Constants
Problem: Missing definitions for ENOMEM, ENOBUFS, EWOULDBLOCK, and EHOSTUNREACH during err.c compilation.
Fix: Set #define LWIP_PROVIDE_ERRNO 1 in lwipopts.h to force lwIP to export its internal POSIX error code table.
4. Interrupt Handling, PLIC Routing & System Lockups
External PLIC Interrupt Routing (portasmHANDLE_INTERRUPT)
Problem: External interrupts were dropping into a default application_interrupt_handler trap in portASM.S instead of being handled by the Microchip PLIC driver.
Fix: Added #define portasmHANDLE_INTERRUPT handle_m_ext_interrupt to FreeRTOSConfig.h and implemented freertos_risc_v_application_interrupt_handler() in main.c as a fallback hook.
UART Interrupt Storm Lockup
Problem: Enabling MMUART0 interrupts on the PLIC without an active ISR clearing the Receiver Buffer Register (RBR) caused continuous interrupt traps that starved the OS tasks.
Fix: Kept MMUART0_E51_INT disabled at the PLIC level and switched UART reception to non-blocking polling inside cli_task.
Invalid Ethernet MAC PLIC IRQ Symbol
Problem: MAC0_INT_E51_INT and PLIC_SetHandler caused unresolved reference errors because they do not exist in the MPFS HAL.
Fix: Switched to the valid MPFS HAL interrupt symbol MAC0_INT_U54_INT using standard PLIC_SetPriority() and PLIC_EnableIRQ() calls.




