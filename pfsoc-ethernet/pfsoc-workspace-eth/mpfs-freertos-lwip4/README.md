lwip + freertos Demo.

Discovery Kit Tested

v4 : added monitor on/off on the webdash baord, increased the stack size of freertos taks, added stackoverflow function for errro reprotign 
v3: added dynamic web page with status information
v2: Added detailed debug messages and help menu
v1 : Basic


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




