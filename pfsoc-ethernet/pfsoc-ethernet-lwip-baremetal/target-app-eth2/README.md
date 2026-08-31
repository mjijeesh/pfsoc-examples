# **PolarFire SoC Bare-Metal lwIP Guide: DDR Target**

This guide details the procedures for building, loading, and debugging bare-metal lwIP networking applications on the PolarFire SoC Discovery Kit (MPFS095T) when targeting DDR memory.

## **1\. System Architecture & Memory Map**

The application runs on **Hart 1 (U54 Core 1\)**. When compiled for the DDR target (MEM\_TARGET=ddr), the executable image resides in cached DDR memory, while Ethernet DMA buffers are mapped to L2 Scratchpad SRAM to ensure hardware coherency.

| Memory Region | Address Range | Memory Type | Usage |
| :---- | :---- | :---- | :---- |
| **ddr\_cached\_32bit** | 0x80000000 \- 0x87FFFFFF (128 MB) | Cached DDR3/4 | Application .text, .data, .bss, .stack, .heap  |
| **scratchpad** | 0x0A000000 \- 0x0A07FFFF (512 KB) | Uncached SRAM | Ethernet DMA Ring Buffers (.uncached\_scrp) |
| **ddr\_non\_cached\_32bit** | 0xC0000000 \- 0xC7FFFFFF (128 MB) | Uncached CPU Alias | CPU direct physical DDR access window |

## **1.1 Software Execution Flags**

> * **\-DIMAGE\_LOADED\_BY\_BOOTLOADER=1**: Disables Microchip HAL BSS initialization in system\_startup.c, assuming the bootloader handles binary loading.  
> * **zero\_bss\_section()**: Explicitly clears uninitialized .sbss and .bss memory from \_\_sbss\_start to \_\_bss\_end upon entering u54\_1() to prevent lwIP pointer crashes.

# **2\. Build System Instructions**

## **2.1 Prerequisites**

> * **Toolchain**: riscv-none-elf-gcc or riscv64-unknown-elf-gcc

> * **Utilities**: Python 3 (for XML hardware header generation), OpenOCD, GDB

## **2.2 Compilation Commands**

Execute the following sequence from the project root to perform a clean build for the DDR target:

**Command:**

```shell
make clean ddr
```

## **2.3 Build Artifacts (build/ddr/)**

After compilation completes, the build system generates the following artifacts in build/ddr/:

| File | Format | Primary Usage |
| :---- | :---- | :---- |
| **pfsoc\_app.elf** | RISC-V ELF Executable | GDB symbolic debugging and JTAG loading |
| **pfsoc\_app.bin** | Raw Binary Image | Bootloader loading into DDR memory |
| **pfsoc\_app.fbi** | Flash Binary Image | SPI Flash / eNVM storage with CRC32 header |
| **pfsoc\_app.hex** | Intel HEX | Flash programming tools |

# **3\. Application Loading & Boot Sequence**

Because executing code in DDR requires active clocking and DDR controller training, a primary bootloader (e.g., LiteX BIOS, HSS, or a custom First Stage Bootloader in eNVM/LIM) must initialize DDR before the application can run.

## **3.1 Boot Sequence Phases**

> 1. **Power On / Reset**: PolarFire SoC resets; Hart 0 (E51) and Hart 1 (U54\_1) start executing bootloader code from eNVM/LIM.  
> 2. **DDR Controller Training**: Bootloader configures MSS DDR PLLs, performs PHY calibration, and maps DDR memory to 0x80000000.  
> 3. **Payload Transfer**: Bootloader fetches pfsoc\_app.bin (via TFTP, UART, SD card, or SPI Flash) and writes the raw image into DDR starting at address 0x80000000.  
> 4. **Execution Handoff**: Bootloader passes Hart ID in a0 and Hart Local Storage in a1, then jumps to 0x80000000.

# **4\. Debugging via JTAG (OpenOCD & GDB)**

**Important**: Do NOT run monitor reset halt in GDB when debugging DDR targets. A full JTAG reset clears the MSS DDR Controller registers, disabling DDR access and causing GDB memory bus errors.


## **4.1 Workflow A: Attach & Load (Binary Deployment)**

Use this workflow to re-compile your C code and load the updated binary directly into DDR over JTAG without losing DDR controller initialization.

> 1. **Power Cycle Board**: Let the bootloader complete DDR training.

**Step 1: Start OpenOCD (Terminal 1\)**

```
make openocd
```

**Terminal Output:**

```
Open On-Chip Debugger 0.12.0
Licensed under GNU GPL v2
Info : Listening on port 3333 for gdb connections
Info : Listening on port 6666 for tcl connections
```

**Step 2: Compile & Push to DDR (Terminal 2\)**

```
make ddrmake debug-ddr
```

**Terminal Output:**

```
(litex-env) jijeesh@jijeesh-Latitude-5300:~/git/pfsoc-examples/pfsoc-ethernet/v1/target-app-eth2$ make debug-ddr
==================================================
 Attaching to Trained DDR Target: build/ddr/pfsoc_app.elf
==================================================
GNU gdb (xPack GNU RISC-V Embedded GCC x86_64) 16.3
Copyright (C) 2024 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later
Remote debugging using localhost:3333
u54_1 () at src/application/hart1/u54_1.c:65
65	    while (1) {
(gdb)
```

**Explanation:**

```
Compiling src/main.c...
Compiling src/lwip_setup.c...
Linking build/ddr/pfsoc_app.elf...
Generating build/ddr/pfsoc_app.bin...
Build Complete.
```

GDB will pause execution, flash pfsoc\_app.elf into DDR over JTAG, reset the PC to \_start, and pause at u54\_1().

## **4.2 Workflow B: Live Attach (System Inspection)**

Use this workflow when the bootloader has already launched the application and you want to inspect variables, set breakpoints, or trace crashes without re-writing memory.  
**Step 1: Start OpenOCD (Terminal 1\)**

```shell
make openocd
```

**Step 2: Connect GDB (Terminal 2\)**

```shell
make attach-ddr
```

**Terminal Output:**

```
GNU gdb (xPack GNU RISC-V Embedded GCC) 16.3
Remote debugging using localhost:3333
0x0000000080001234 in main_loop ()
(gdb) interrupt
Thread 2 hit Breakpoint 1, 0x80001234 in main_loop ()
```

> 1. Control execution directly from the GDB prompt.


## **5. Hardware Hart & Thread Mapping**

The Microchip PolarFire SoC features a 5-core RISC-V CPU complex (1x E51 Monitor Core \+ 4x U54 Application Cores). When attached via GDB and OpenOCD on port 3333, each physical core is exposed as an OS Thread:

| GDB Thread ID | Hardware Core | Architecture | Default Target Role / Execution State |
| :---- | :---- | :---- | :---- |
| **Thread 1** | **Hart 0** | RV64IMAC (E51) | System Primary Core; loops in wait\_main\_hart() inside mss\_entry.S. |
| **Thread 2** | **Hart 1** | RV64GC (U54\_1) | **Active Application Host**; runs u54\_1() and the main mac\_task() lwIP loop. |
| **Thread 3** | **Hart 2** | RV64GC (U54\_2) | Secondary Core; parked in wfi (Wait For Interrupt) inside u54\_2.c. |
| **Thread 4** | **Hart 3** | RV64GC (U54\_3) | Secondary Core; parked in wait\_main\_hart() inside mss\_entry.S. |
| **Thread 5** | **Hart 4** | RV64GC (U54\_4) | Secondary Core; parked in wait\_main\_hart() inside mss\_entry.S. |

# **6\. Detailed Debugging Trace & Analysis**

Executing make debug-ddr attaches GDB to an active OpenOCD server (localhost:3333) on a board that has already undergone DDR training via the bootloader. GDB re-writes the ELF sections directly into active DDR memory without triggering a hardware reset.

Plaintext

```
Remote debugging using localhost:3333
0x0000000020227100 in ?? ()
Loading section .text, size 0x2a350 lma 0x80000000
Loading section .sdata, size 0x40 lma 0x8002a350
Loading section .data, size 0xe30 lma 0x8002a390
Start address 0x0000000080000000, load size 176576
```

### **Memory Allocation Breakdown**

* **Pre-Load State (0x20227100)**: Core execution is halted inside the eNVM Bootloader execution range.  
* **Application Load Address (0x80000000)**: The .text code segment, .sdata, and initialized .data segments are written into physical DDR (0x80000000 to 0x8002B1C0).  
* **Total Binary Image Payload**: 176,576 bytes loaded at 19 KB/s over JTAG.

## **Step-by-Step Debugging Execution Trace**

## **6.1 Reaching Entry Point (u54\_1)**

**Command:**

```
continue
```

**Terminal Output:**

```
(gdb) c
Continuing.
[Switching to Thread 2]

Thread 2 hit Breakpoint 1, u54_1 () at application/hart1/u54_1.c:703
703         zero_bss_section();
```

**Explanation:** Hart 1 hits the first breakpoint. zero\_bss\_section() ensures memory is cleared before lwIP stack initialization.

## **6.2 Inspecting Active Subroutines**

**Command:**

```
interrupt
finish
```

**Terminal Output:**

```
(gdb) finish
Run till exit from #0  sys_check_timeouts () at middleware/lwip/core/timeouts.c:401
mac_task (pvParameters=0x0) at application/hart1/u54_1.c:573
573             check_phy_link_status();
Value returned is $1 = 90
```

**Explanation:** The trace shows the system returning from sys\_check\_timeouts(). A return value of 90 indicates 90ms until the next lwIP timer event.

## **6.3 Live Variable Diagnostics**

Switching GDB context to Thread 3 (Hart 2\) confirms secondary cores are held in low-power sleep states:

Plaintext

```
(gdb) thread 3
[Switching to thread 3 (Thread 3)]
#0  0x0000000080002b4a in u54_2 () at application/hart2/u54_2.c:9
9           __asm__ volatile ("wfi");

(gdb) bt
#0  0x0000000080002b4a in u54_2 () at application/hart2/u54_2.c:9
#1  0x0000000080023dc6 in main_other_hart (hls=0x800d7fc0) at platform/mpfs_hal/startup_gcc/system_startup.c:417
```

*   
  **Hart 2 Status**: Parked in wfi inside u54\_2() after passing through main\_other\_hart() in system\_startup.c.

### **4\. Real-Time Network Stack & Variable Diagnostics**

**Command:**

```
print g_tick_counter
print/x g_netif.ip_addr
```

#### **System Millisecond Tick Inspection**

**Terminal Output:**

```
(gdb) thread 2
[Switching to thread 2 (Thread 2)]
(gdb) print g_tick_counter
$2 = 96825
```

**Explanation:** Verifies system uptime and decodes the current IP address from little-endian format (e.g., 0x2c14a8c0 \= 192.168.20.44).  
Inspecting the physical DDR reset vector at 0x80000000 displays raw RISC-V machine instructions loaded by GDB:

Plaintext

```
(gdb) x/16xw 0x80000000
0x80000000 <reset_vector>:    0x00000717    0x20070713    0x30571073    0x305027f3
0x80000010 <reset_vector+16>: 0xfef71ee3    0x0001e2b7    0x3002a073    0xf1402573
0x80000020 <reset_vector+32>: 0x00050463    0x00301073    0x301022f3    0x0002c463
0x80000030 <.no_float+8>:     0xff9ff06f    0x0002b197    0xb1c18193    0xf1402573
```

*   
  **Opcode Verification**: Words starting at 0x80000000 represent the compiled boot entry vector (auipc, csrw mtvec, csrr mhartid) defined in mss\_entry.S.

# **7\. Known Diagnostic Warnings & Fixes**

## **7.1 Resolving max-value-size Structure Limits**

When attempting to print large driver instances like print \*g\_test\_mac, GDB outputs:

**Terminal Error:**

```
value of type `mss_mac_instance_t' requires 133136 bytes, which is more than max-value-size
```

**Fix:**  
Disable the memory evaluation limit in GDB:  
**Command:**

```
(gdb) set max-value-size unlimited
(gdb) print g_test_mac->mac_addr
$4 = {0x00, 0xfc, 0x00, 0x12, 0x34, 0x58}
(gdb) print g_test_mac->queue[0].nb_available_tx_desc
$5 = 8
```

# **Application Usage Instructions**

## **Serial Console Connection**

Connect a micro-USB cable to the board's UART interface and open a serial terminal emulator (e.g., Minicom, Tera Term, PuTTY).

> * **Port**: Interactive MMUART1 (\&g\_mss\_uart1\_lo)  
     **Baud Rate**: 115200
     **Data Bits**: 8
     **Parity**: None
    **Stop Bits**: 1
    **Flow Control**: None

Upon boot or reload, the board displays the initialization banner and CLI prompt (eth-cli\>):

```
==========PolarFire SoC - Bare-Metal lwIP Engine =================
[DEBUG] lwIP Network Stack Initialized Successfully.
[DHCP] Initiating Auto-DHCP Discovery (10s Timeout limit)...
[DEBUG] HTTP Web Server (Port 80) Started.
[DEBUG] iPerf Bandwidth Server (Port 5001) Started.
Type 'help' for available CLI commands.
eth-cli> 
```

```
eth-cli> status

[NETWORK INTERFACE STATUS]
  Interface Name : e0
  Hardware (MAC) : 00:FC:00:12:34:58
  Link Status    : UP (Connected)
  IPv4 Address   : 192.168.20.44
  Subnet Mask    : 255.255.255.128
  Default Gateway: 192.168.20.1
  DHCP Client    : BOUND (Active)
  Traffic Monitor: DISABLED (OFF)
  Tx Frame Count : 7
  Rx Frame Count : 656

```

## **Interactive Command Reference**

| Command | Usage Syntax | Description |
| :---- | :---- | :---- |
| **help** | help or ? | Displays the command menu and active background service ports. |
| **status** | status | Displays interface name, MAC address, IPv4 address, Subnet Mask, Default Gateway, DHCP lease state, Traffic Monitor toggle, and total Hardware TX/RX frame counters. |
| **dhcp** | dhcp | Manually triggers a DHCP DISCOVER request to acquire a dynamic IP from the router. |
| **ping** | ping \<ip1.ip2.ip3.ip4\> | Transmits an outbound ICMP Echo Request frame to a target IPv4 address. |
| **arp** | arp \<ip1.ip2.ip3.ip4\> | Sends a Layer 2 ARP Request frame to resolve the hardware MAC address of a target IP. |
| **monitor** | monitor \<on|off\> | Enallows or disables real-time background packet inspection prints on the CLI console. |

## **IP Allocation Behavior & Fallback**

> 1. **Auto-DHCP at Boot**: The application initiates a DHCP DISCOVER request automatically upon system startup.  
> 2. **10-Second Timeout Fallback**: If no DHCP server responds within 10 seconds (10000 ms), the stack stops DHCP and assigns the default static network configuration:  
   * **IPv4 Address**: 192.168.20.44
   * **Subnet Mask**: 255.255.255.0
   * **Default Gateway**: 192.168.20.1

> 3. **Manual DHCP Trigger**: Executing dhcp at any time re-initiates discovery. If bound, it clears the old address and requests a new lease.

## **Service Testing Procedures**

### **1\. HTTP Web Server (Port 80\)**

> 1. Connect the board to your local network router or directly to a host PC via Ethernet.  
> 2. Verify the board's active IP using status.  
> 3. Open a web browser on your host computer and navigate to http://\<board\_ip\> (e.g., \[http://192.168.20.44\](http://192.168.20.44)).

The dashboard page will render. The terminal will display a real-time event notification:

```
[HTTP EVENT] Web server page accessed by 192.168.20.100
```

2\. ICMP Ping Testing  
Inbound Ping (Host PC $\\rightarrow$ Board): Run ping \<board\_ip\> from your host terminal. The CLI outputs an event for incoming requests:

```
[PING EVENT] Inbound Ping Request received from 192.168.20.100
```

Outbound Ping (Board $\\rightarrow$ Host PC): Execute ping \<target\_ip\> in the board's CLI:

```
eth-cli> ping 192.168.20.1
[DEBUG] Transmitting ICMP Echo Request to 192.168.20.1 (Seq: 1)
[DEBUG] ICMP Echo Reply received from 192.168.20.1 (Seq: 1)
```

### **3\. iPerf Bandwidth Benchmark (Port 5001\)**

> 1. Ensure the board is in BOUND DHCP or active static IP mode.

Run an iPerf TCP test from the host PC targeted at Port 5001:

```
iperf -c <board_ip> -p 5001 -i 1
```

The board logs the incoming connection request and outputs a completion summary report:

```
[IPERF EVENT] Connection request on Port 5001 from 192.168.20.100
[IPERF REPORT] Transferred: 10240 KB | Duration: 1001 ms | Bandwidth: 8.83 Mbps
```


