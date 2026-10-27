# **PolarFire SoC Bootloader**

## **Prerequisites & Toolchain Setup**

Before building the project, configure the toolchain installation paths in your `Makefile` to match your local system.

Makefile

```
ifeq ($(OS),Windows_NT)
    PYTHON        ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/python3/python.exe
    JAVA          ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/eclipse/jre/bin/java.exe
    OPENOCD       ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/openocd/bin/openocd.exe
    SC_INSTALL_DIR ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747
    FPGENPROG     ?= C:/Microchip/Libero_SoC_2025.2/Libero_SoC/Designer/bin64/fpgenprog    
else
    PYTHON        ?= python3
    JAVA          ?= java
    OPENOCD       ?= /opt/microchip/SoftConsole/SoftConsole-v2022/openocd/bin/openocd
    SC_INSTALL_DIR ?= /opt/microchip/SoftConsole/SoftConsole-v2022
    FPGENPROG     ?= /opt/microchip/Libero_SoC_2025.2/Libero_SoC/Designer/bin64/fpgenprog    
endif
```

## **Complete Build and Execution Workflow**

### **1\. Build the Bootloader**

The BIOS Bootloader is compiled to reside in non-volatile eNVM memory and execute out of LIM (L2 Scratchpad Memory).

* **MPFS Discovery Kit:**

```
make clean envm-lim BOARD=mpfs-discovery-kit
```

* **MPFS Icicle Kit:**

```
make clean envm-lim BOARD=mpfs-icicle-kit
```

### **2\. Program the eNVM Memory (Boot Mode 1\)**

Execute the programming command for your target board:

```
make program-envm-lim BOARD=mpfs-discovery-kit
```

**Expected Programming Output:**

```
==================================================
 Programming eNVM-LIM Scratchpad (Boot Mode 1)
 Target Device : MPFS095T (FCSG325)
 Working Dir   : C:/Users/jijeesh/git/pfsoc-examples/pfsoc-ethernet/mpfs-bootloader/build/envm-lim
==================================================
"C:/Microchip/SoftConsole-v2022.2-RISC-V-747/eclipse/jre/bin/java.exe" -jar C:/Microchip/SoftConsole-v2022.2-RISC-V-747/extras/mpfs/mpfsBootmodeProgrammer.jar \
        --bootmode 1 \
        --die MPFS095T \
        --package FCSG325 \
        --workdir C:/Users/jijeesh/git/pfsoc-examples/pfsoc-ethernet/mpfs-bootloader/build/envm-lim
17:09:51 INFO  - mpfsBootmodeProgrammer v3.7 started.
17:09:52 INFO  - "C:\Users\jijeesh\git\pfsoc-examples\pfsoc-ethernet\mpfs-bootloader\build\envm-lim\bootmode1" is the output folder and the previous contents of this folder will be deleted.
17:09:52 INFO  - Selected boot mode "1 - non-secure boot from eNVM" and working in directory "C:\Users\jijeesh\git\pfsoc-examples\pfsoc-ethernet\mpfs-bootloader\build\envm-lim".
17:09:52 INFO  - Generating BIN file...
17:09:52 INFO  - Generating header...
17:09:52 INFO  - Generating HEX file...
17:09:52 INFO  - Preparing for bitstream generation...
17:09:52 INFO  - Generating bitstream...
17:10:03 INFO  - Programming the target...
17:10:11 INFO  - mpfsBootmodeProgrammer completed successfully.
```

### **3\. Connect to the Bootloader Terminal**

Connect to the target console using `mpfs-terminal.py`. This utility provides terminal access and manages SFL binary uploads into DDR RAM or SPI Flash.

```
python mpfs-terminal.py COM6
```

In the terminal prompt, type `help` to display available commands:

```
[MPFS-TERM] Opened COM6 at 115200 baud. (Press Ctrl+C to exit)
[MPFS-TERM] Type 'serialboot' on prompt to trigger download.

help

 MPFS BIOS, available commands:

-- System Commands --
  help             - Print available commands
  ident            - Identifier of the system
  hw_info          - Display hardware map and peripherals
  reboot           - Reboot system
  sys_serial       - Read 128-bit Device Serial Number (DSN)
  sys_info         - Read FPGA Usercode and Design Information
  sys_iap          - Execute IAP Bitstream Programming: sys_iap <addr>
  sys_digest       - Run Digest Integrity Check across fabric & memories

-- Boot Commands --
  boot             - Boot from Memory: boot <addr> [r1]
  serialboot       - Boot from Serial (SFL)
  flash_idcode     - Read SPI Flash JEDEC Manufacturer & Device ID
  flashwrite       - Upload binary to SPI Flash over SFL
  flashboot        - Boot FBI image from SPI Flash: flashboot [offset] [ram_addr]
  flash_write      - Write RAM to Flash: flash_write <offset> <ram_addr> [count]
  flash_erase_range- Erase Flash range: flash_erase_range <offset> <count>
  flash_read       - Read Flash: flash_read <offset> [count]
  flash_copy       - Copy Flash to RAM: flash_copy <offset> <ram_addr> [count]

-- Memory Commands --
  mem_read         - Read memory: mem_read <addr> [len]
  mem_write        - Write memory: mem_write <addr> <val>
  mem_copy         - Copy memory: mem_copy <dst> <src> [n]
  mem_test         - Test memory access: mem_test <addr>
  mem_cmp          - Compare memory: mem_cmp <a1> <a2> <n>

mpfs-discovery-kit> flash_idcode
[SPI FLASH] Auto-detecting Flash on MikroBus...
[SPI FLASH] Detected: SST25 Series Flash (Mfr: 0x62, Dev: 0x06, Cap: 0x13) [512 KB (524288 bytes)]
Active SPI Driver: SST25 Series Flash
SPI Flash JEDEC Identification:
  - Manufacturer ID : 0x62
  - Device ID       : 0x06
  - Capacity ID     : 0x13
  - Detected Size   : 512 KB (524288 bytes)
```

## **Application Deployment Workflows**

### **Option A: Execute Application from DDR Memory (`.bin`)**

The bootloader trains the DDR controller at startup. You can stream a raw binary file (`.bin`) directly to DDR RAM (`0x80000000`) over serial and launch it immediately.

* **File Format Rule:** Standard serialboot execution requires a `.bin` image.

Bash

```
python mpfs-terminal.py COM6 --kernel .\build\ddr\pfsoc_app.bin --kernel-adr 0x80000000 --serial-boot
```

* `--kernel`: Path to the application binary (`.bin`).  
* `--kernel-adr`: Target memory destination (default: `0x80000000`).  
* `--serial-boot`: Automatically triggers the `serialboot` command on connection.

**Console Execution Sequence:**

```
[MPFS-TERM] Opened COM6 at 115200 baud. (Press Ctrl+C to exit)
[MPFS-TERM] Type 'serialboot' on prompt to trigger download.

serialboot

Waiting for serialboot stream from host...

Press Q or ESC to abort.
sL5DdSMmkekro

[MPFS-TERM] Magic trigger matched! Replying with handshake ACK...
[MPFS-TERM] Uploading .\build\ddr\pfsoc_app.bin (207408 bytes) -> 0x80000000...
|====================| 100.0% (207408/207408 B)
[MPFS-TERM] Transfer Complete (7.2 KB/s).
[MPFS-TERM] Sending JUMP Command to boot application...
[MPFS-TERM] Terminal active. Continuing console output...

Launching target application on U54 harts at 0x0000000080000000...

E51 Monitor Core remains active in BIOS.
```

> **Note on UART Assignment:** The BIOS Bootloader reserves `MSS_UART0`. Ensure user applications utilize `MSS_UART1` or `MSS_UART2`. Connect a separate terminal to the corresponding UART port to interact with application-level CLIs (e.g., FreeRTOS / lwIP console):

```
eth-cli> status

[NETWORK STATUS]
  Execution    : FreeRTOS Task Kernel Active
  Interface    : e0
  MAC          : 00:FC:00:12:34:58
  Link         : UP
  IPv4 Address : 192.168.20.45
  Subnet Mask  : 255.255.255.128
  Gateway      : 192.168.20.1
  DHCP State   : BOUND (Dynamic IP)
  Traffic Mon  : DISABLED (OFF)
  Tx Frames    : 40
  Rx Frames    : 5575
```

### **Option B: Program Application to SPI Flash Memory (`.fbi`)**

To persist an application across power cycles, flash a Formatted Binary Image (`.fbi`) containing the FBI header into SPI NOR Flash. At boot, the BIOS copies this payload to DDR RAM and verifies its CRC32 checksum before execution.

Note: when building the target application, an `.fbi with prefixing binary size and CRC will be generated by the build system This is done using the crcfbigen.py file`

* **File Format Rule:** SPI Flash mode (`--flash`) **requires** an `.fbi` image.  
* **Target Address Offsets:**  
  * **MPFS Discovery Kit:** `0x00002000` (`0x2000`)  
  * **MPFS Icicle Kit:** `0x00100000` (`0x100000`)  
* **Programming Command (Discovery Kit):**

```
python mpfs-terminal.py COM6 --kernel .\build\ddr\pfsoc_app.fbi --kernel-adr 0x2000 --flash
```


* **Programming Command (Icicle Kit):**

```
python mpfs-terminal.py COM6 --kernel .\build\ddr\pfsoc_app.fbi --kernel-adr 0x00100000 --flash
```

Once the terminal connects, issue the `flashwrite` command to initiate flashing:

```
mpfs-discovery-kit> flashwrite

=======================================================
 [FLASHWRITE] Direct SPI Flash Flashing Mode
=======================================================

Waiting for serialboot stream from host...

Press Q or ESC to abort.
sL5DdSMmkekro

[MPFS-TERM] Magic trigger matched! Replying with handshake ACK...
[MPFS-TERM] Requesting SPI Flash erase/prep (207408 bytes) at Offset 0x00002000...
[MPFS-TERM] Uploading .\build\ddr\pfsoc_app.fbi (207408 bytes) -> 0x00002000...
|====================| 100.0% (207408/207408 B)
[MPFS-TERM] Transfer Complete (2.2 KB/s).
[MPFS-TERM] Sending DONE command...
```

After programming completes:

1. Exit `mpfs-terminal.py` using `Ctrl+C` twice (to prevent triggering re-flashing on reconnect).  
2. Re-open terminal mode: `python mpfs-terminal.py COM6`  
3. Execute `flashboot` to test immediate boot from SPI Flash, or issue `reboot` to test power-on autoboot.

