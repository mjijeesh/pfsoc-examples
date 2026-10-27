# **PolarFire SoC Ethernet PHY Diagnostic Application**

A bare-metal network diagnostic suite and lightweight HTTP server targeting Microchip PolarFire SoC (MPFS) platforms. It provides low-level driver validation, SGMII/PHY hardware loopback testing, frame inspection, and MDIO register manipulation.

## **Key Features**

* **Target Execution:** Runs on the **E51 Monitor Core** by default (hart0). *Note: To port execution to a U54 core (hart1–hart4), update the MAC and Timer IRQ vector mappings.*  
* **Network Protocol Support:** Embedded status Web Server (Port 80), ICMP Echo Request/Reply (Ping), and dynamic DHCP address assignment.  
* **Hardware Loopback Modes:** Supports GEM PCS SGMII loopback and PHY near-end loopback.  
* **MDIO & PHY Utilities:** MDIO bus scanning (addresses 0–31), raw 16-bit register R/W, and full PHY register dumping.  
* **Analysis & Packet Testing:** Raw Layer 2 frame generator, packet decoder/inspector, side-by-side TX/RX buffer diff, DMA descriptor queue inspector, and MAC MIB hardware statistics.

## **Build & Debug Workflow**

### **1\. Launch OpenOCD Debug Server**

Connect your FlashPro or embedded programmer, then launch OpenOCD in a dedicated terminal:

```
make openocd
```

### **2\. Option A: LIM Execution (L2 Scratchpad Memory)**

> **Linker Requirement:** Ensure your LIM linker script allocates at least **1 MB** of LIM space.

```
# Clean and compile for LIM
make clean lim

# Launch GDB debug session
make lim-debug   # or 'make debug'
```

### **3\. Option B: DDR Execution (Fresh Download)**

> **Pre-requisite:** Ensure board DDR training has completed before launching. Do **not** issue a monitor reset command in GDB when debugging from DDR.

```
# Clean and compile for DDR
make clean ddr

# Download binary to DDR and launch GDB session
make debug-ddr
```

### **4\. Option C: Attach to Application Running in DDR**

If the bootloader has already trained DDR and loaded the ELF file into RAM, attach GDB directly without re-downloading the binary:

```
make attach-ddr
```

## **Diagnostic CLI Reference (eth-disco\>)**

Interact with the diagnostic engine over MSS\_UART0 (115200 8N1). Type help to display the full menu.

### **Command Overview**

| Command | Description |
| :---- | :---- |
| status | Displays MAC/IP configuration, link state, and driver packet counters |
| ping \<ip\> | Transmits ICMP Echo Requests to the target host |
| dhcp | Transmits a DHCP Discover broadcast to request a dynamic IPv4 address |
| mdio scan | Probes MDIO addresses 0–31 for attached PHY devices |
| mdio read \<reg\> | Reads a 16-bit register from the active PHY |
| mdio write \<reg\> \<val\> | Writes a 16-bit value to a PHY register |
| loopback \<pcs|phy|off\> | Controls GEM PCS SGMII and PHY near-end hardware loopbacks |
| stats | Displays hardware MIB statistics (TX/RX octets, CRC, alignment errors) |
| gem dump / phy dump | Dumps register ranges for GEM MAC (0x000–0x084) or PHY (0x00–0x0F) |

### **Example Diagnostic Session**

```
eth-disco> status
[STATUS] MAC Addr: 00:FC:00:12:34:58 | IP Addr: 192.168.20.107
[STATUS] Link: UP | Speed: 1Gbps | Duplex: Full
[STATUS] Transmitted Packets: 23 | Received Packets: 3769

eth-disco> ping 192.168.20.11
[PING] Sending 32 bytes to 192.168.20.11 [8A:54:C1:AE:26:11]...
[PING] Reply from 192.168.20.11: bytes=40 time=10 ms

eth-disco> mdio scan

--- Scanning MDIO Bus via GEM0 (Addresses 0-31) ---
  [FOUND] Addr 0x0B | ID: 0x000F:0xC551 <-- Active VSC8221
-------------------------------------------
Active PHY: 0x0B

eth-disco> dhcp
[DHCP] Transmitting DHCP Discover Broadcast...
[DHCP] Listening for DHCP Offer on UDP Port 68...

================ DHCP OFFER RECEIVED ================
Offered IP Addr : 192.168.20.45
Server IP Addr  : 192.168.20.1
=====================================================
```

