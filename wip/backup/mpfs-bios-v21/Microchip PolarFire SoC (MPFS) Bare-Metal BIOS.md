# **Microchip PolarFire SoC (MPFS) Bare-Metal BIOS & Target Application Framework**

This repository provides an integrated bare-metal software framework for the Microchip PolarFire SoC (MPFS). It features a LiteX-ported primary BIOS/bootloader running on the E51 monitor core (Hart 0\) and an isolated target application framework designed for the U54 application core (Hart 1).

## **1\. System Overview & Architecture**

### **Multi-Hart Execution Model**

> * **E51 Monitor Core (Hart 0\)**: Runs the LiteX BIOS shell, handles command-line interfaces, memory diagnostics, and binary receiver protocols (XMODEM and SFL).  
> * **U54\_1 Application Core (Hart 1\)**: Executes target application payloads. HAL settings (MPFS\_HAL\_FIRST\_HART \= 1, MPFS\_HAL\_LAST\_HART \= 2\) restrict execution to Hart 1 when operating in application mode.  
> * **Inter-Processor Boot Handoff**: When launching an application, Hart 0 writes the target jump address to g\_app\_jump\_addr, issues an instruction pipeline fence (fence), and triggers an Inter-Processor Interrupt (raise\_soft\_interrupt()) to unpark Hart 1\. Hart 0 then safely returns to the BIOS shell prompt (litex\>).

### **Isolated Dual-UART Mapping**

To prevent console cross-talk between the BIOS and the target application, standard I/O is hard-routed to separate physical UART peripherals:

| Subsystem | Target Processor | Hardware Peripheral | Driver Pointer | Linux Serial Device |
| :---- | :---- | :---- | :---- | :---- |
| **BIOS / Bootloader** | E51 (Hart 0\) | MMUART0 (MSS\_PERIPH\_MMUART\_E51) | p\_uartmap\_e51 (\&g\_mss\_uart0\_lo) | /dev/ttyUSB0  |
| **Target Application** | U54\_1 (Hart 1\) | MMUART1 (MSS\_PERIPH\_MMUART\_U54\_1) | p\_uartmap\_u54\_1 (\&g\_mss\_uart1\_lo) | /dev/ttyUSB1  |

## **2\. Memory Target Architecture & Build Configurations**

The build system (Makefile) supports five distinct memory allocation targets depending on the deployment role:

| Memory Target | Subsystem Role | IMAGE\_LOADED\_BY\_BOOTLOADER | MPFS\_HAL\_HW\_CONFIG | Primary Linker Script | Memory Execution Space |
| :---- | :---- | :---- | :---- | :---- | :---- |
| **envm** | Direct XIP BIOS | 0  | Defined | mpfs-envm.ld  | Direct Execute-in-Place (XIP) from eNVM Flash. |
| **envm-lim** | eNVM-to-LIM FSBL | 0  | Defined | mpfs-envm-lma-scratchpad-vma.ld  | Flash LMA in eNVM (0x20220000); copied to L2-LIM RAM (0x08000000) at reset. |
| **lim** | JTAG Debug BIOS | 0  | Defined | mpfs-lim.ld  | Lower L2-LIM RAM (0x08000000) loaded directly via JTAG. |
| **lim-app** | Target Application | 1  | Not Defined | mpfs-lim-app.ld  | Upper L2-LIM RAM (0x08040000) loaded via BIOS or serialboot. |
| **ddr** | Target Application | 1  | Not Defined | mpfs-ddr-loaded-by-boot-loader.ld  | Main DDR RAM (0x80000000) loaded via BIOS or serialboot. |

### **Deep Dive: envm-lim (eNVM Scratchpad Zero-Stage Bootloader)**

The envm-lim build target configures the application to act as a **First Stage Bootloader (FSBL) / Standalone BIOS**:

> * **Cold Boot Initialization (IMAGE\_LOADED\_BY\_BOOTLOADER \= 0\)**: Tells the HAL to perform complete SoC bring-up from reset.  
> * **Hardware Configuration (MPFS\_HAL\_HW\_CONFIG)**: Automatically defined to initialize L2 cache ways, clock PLLs, bus error units (BEU), MPU, PMP, and DDR PHY memory training.  
> * **Hart Range (MPFS\_HAL\_FIRST\_HART \= 0, MPFS\_HAL\_LAST\_HART \= 4\)**: Boots Hart 0 (E51) first and retains management over all four U54 application harts.  
> * **LMA vs VMA Split**: Non-volatile code resides in eNVM flash (Load Memory Address) and is copied into fast L2-LIM scratchpad RAM (Virtual Memory Address) during early assembly startup.

## **3\. Subsystem Implementation & Software Architecture**

### **1\. Interactive Command Shell (cli.c, command.c, readline.c)**

> * **ANSI/VT100 Line Editor**: Supports cursor navigation, clear screen, character deletion, line editing, and a command history ring buffer.  
> * **Command Table Dispatch**: Scans input tokens against an internal registration table to execute system, memory, or boot callbacks.

### **2\. Built-in Shell Commands**

| Category | Command | Syntax / Usage | Functional Description |
| :---- | :---- | :---- | :---- |
| **System**  | help  | help  | Displays categorized list of all available commands. |
|  | ident  | ident  | Outputs system identification and target core context. |
|  | hw\_info  | hw\_info  | Prints system clock frequencies, memory map, Libero design settings, and peripheral status. |
|  | reboot  | reboot  | Triggers a full system soft reset by writing 0xDEAD to SYSREG-\>MSS\_RESET\_CR (0x20002018). |
| **Boot**  | boot  | boot \<hex\_addr\> \[r1\]  | Launches target application at \<hex\_addr\> across U54 cores via IPI. |
|  | serialboot  | serialboot  | Initiates Serial Framing Protocol (SFL) handshake for automated binary download. |
|  | go  | go \<hex\_addr\>  | Flushes serial FIFO and directly jumps execution to specified address. |
| **Memory**  | mr / mem\_read  | mr \<hex\_addr\> \[count\]  | Dumps memory contents formatted as 32-bit hex dwords. |
|  | mw / mem\_write  | mw \<hex\_addr\> \<hex\_val\>  | Writes a 32-bit word value directly to a memory address. |
|  | mem\_copy  | mem\_copy \<dst\> \<src\> \[words\]  | Copies words 32-bit words from source to destination. |
|  | mem\_test  | mem\_test \<addr\> \[bytes\]  | Performs memory bus and pattern check using address-as-data verification. |
|  | mem\_cmp  | mem\_cmp \<a1\> \<a2\> \<words\>  | Compares two memory blocks word-by-word and reports mismatches. |
| **Transfer**  | xmodem  | xmodem \<hex\_addr\>  | Receives binary file over UART using XMODEM-CRC into \<hex\_addr\>. |

### **3\. Binary Transfer Engines (xmodem.c, boot.c)**

> * **SFL Protocol (boot.c)**: Sends SFL\_MAGIC\_REQ sequence to connect with litex-term. Parses payload frames, validates 16-bit CRC checks, writes data to memory, and triggers multi-hart jumps.  
> * **XMODEM-CRC (xmodem.c)**: Uses ASCII 'C' sync negotiation to enforce 16-bit CRC CCITT error checking over 128-byte (SOH) and 1024-byte (STX) packet frames.  
> * **Address Safety Validation (boot\_load\_max\_size)**: Restricts writes strictly to valid hardware memory ranges: L2-LIM Scratchpad (0x08000000–0x08200000), DDR Cached (0x80000000–0xC0000000), or DDR Non-Cached (0xC0000000–0xE0000000).

### **4\. Target Application Driver & Syscall Binding (uart.c)**

> * **Clock Gating**: Calls mss\_config\_clk\_rst() to explicitly enable clocking for MSS\_PERIPH\_MMUART\_U54\_1.  
> * **Non-Blocking Buffering**: Maintains a 1-character lookahead buffer (lookahead\_char) for non-blocking UART polling.  
> * **Newlib Redirection**: Overrides standard C system calls \_write() and \_read1() to redirect printf and getchar directly to MMUART1.

## **4\. Hardware & Toolchain Prerequisites**

> * **Toolchain**: riscv-none-elf-gcc or riscv64-unknown-elf-gcc installed in system PATH.  
> * **Software Tooling**: SoftConsole v2022 / Libero SoC 2025.2 (OpenOCD, fpgenprog, and mpfsBootmodeProgrammer.jar).  
> * **Python**: python3 required for XML-to-header SoC configuration generation (mpfs\_configuration\_generator.py).  
> * **Terminal Emulator**: picocom, minicom, or litex-term supporting XMODEM file uploads.

## **5\. Build & Deployment Workflows**

### **1\. Compiling Software Targets**

To build the primary BIOS or application target executables:

```shell
# Build BIOS / Bootloader for L2-LIM RAM (0x08000000)make clean lim

# Build Standalone Boot Mode 1 eNVM-to-LIM Bootloadermake clean envm-lim# Build Target Application for upper L2-LIM RAM (0x08040000)make clean lim-app# Build Target Application for main DDR RAM (0x80000000)make clean ddr
```

### **2\. Programming eNVM Flash (Boot Mode 1\)**

To flash an envm-lim or envm binary directly to non-volatile eNVM flash:

```shell
make program-envm-lim
```

This rule triggers mpfsBootmodeProgrammer.jar to write the payload to eNVM flash (0x20220000) in Boot Mode 1\.

### **3\. Loading Applications via Serial Boot Protocols**

#### **Method A: Automated Upload with serialboot (litex-term)**

Open a terminal on Terminal 1 (MMUART0 / /dev/ttyUSB0):

```shell
litex-term /dev/ttyUSB0 --speed 115200 --kernel build/ddr/pfsoc_app.bin --kernel-adr 0x80000000
```

> 1. Open a second terminal on **Terminal 2** (MMUART1 / /dev/ttyUSB1) to monitor the target application output.

In the BIOS prompt on Terminal 1, run:

```shell
litex> serialboot
```

> 2. litex-term detects the SFL handshake, streams the binary payload into memory, and triggers execution on Hart 1\. Application output appears on Terminal 2\.

### **3\. Hardware Debugging via OpenOCD & GDB**

Start the JTAG OpenOCD server:

```shell
make openocd
```

In a separate window, launch GDB to attach symbols and load the binary directly over JTAG:

```shell
make debug
```

