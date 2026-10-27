Latest Version Of  Ethernet PHy Debug

features :

Implments the simple  web server, ping response,initate. Also loopback for phy/pcs layer.
Also  option to read/write phy register etc.

The application is intend to run from e51 core. if needed to run fro many other core, change the IRQ numbers for the MAC/timer etc.




From the cli we can initiate a ping and icp command..


Building instruction : LIM.

Make sure taht your linkler fiel has enough LIM space ( more tahn 1MB).


# start the openocd server

$ make openocd 

```
C:/Microchip/SoftConsole-v2022.2-RISC-V-747/openocd/bin/openocd.exe -c "set DEVICE MPFS" -f board/microsemi-riscv.cfg
xPack OpenOCD (Microchip SoftConsole build), x86_64 Open On-Chip Debugger 0.10.0+dev-00859-g95a8cd9b5-dirty (2022-03-15-14:08)
Licensed under GNU GPL v2
For bug reports, read
        http://openocd.org/doc/doxygen/bugs.html
MPFS
Info : only one transport option; autoselect 'jtag'
Info : Hardware thread awareness created
do_board_reset_init
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : No Embedded FlashPro6 (revision B) devices found
fpServer v17 waiting for incoming connections on the port 3334 with API v5
Info : 1 1788414321907 microsemi_flashpro_server.c:1751 microsemi_flashpro_initialize() FlashPro ports available: E203ASSHWY
Info : 2 1788414321907 microsemi_flashpro_server.c:1752 microsemi_flashpro_initialize() FlashPro port selected:   E203ASSHWY
Info : clock speed 6000 kHz
Info : JTAG tap: mpfs.cpu tap/device found: 0x0f8181cf (mfg: 0x0e7 (GateField), part: 0xf818, ver: 0x0)
Info : datacount=2 progbufsize=16
Info : Disabling abstract command reads from CSRs.
Info : Core 0 could not be made part of halt group 1.
Info : Examined RISC-V core; found 5 harts
Info :  hart 0: XLEN=64, misa=0x8000000000101105
Info :  hart 1: currently disabled
Info :  hart 2: currently disabled
Info :  hart 3: currently disabled
Info :  hart 4: currently disabled
Info : datacount=2 progbufsize=16
Info : Disabling abstract command reads from CSRs.
Info : Core 1 could not be made part of halt group 1.
Info : Examined RISC-V core; found 5 harts
Info :  hart 0: currently disabled
Info :  hart 1: XLEN=64, misa=0x800000000014112d
Info :  hart 2: currently disabled
Info :  hart 3: currently disabled
Info :  hart 4: currently disabled
Info : datacount=2 progbufsize=16
Info : Disabling abstract command reads from CSRs.
Info : Core 2 could not be made part of halt group 1.
Info : Examined RISC-V core; found 5 harts
Info :  hart 0: currently disabled
Info :  hart 1: currently disabled
Info :  hart 2: XLEN=64, misa=0x800000000014112d
Info :  hart 3: currently disabled
Info :  hart 4: currently disabled
Info : datacount=2 progbufsize=16
Info : Disabling abstract command reads from CSRs.
Info : Core 3 could not be made part of halt group 1.
Info : Examined RISC-V core; found 5 harts
Info :  hart 0: currently disabled
Info :  hart 1: currently disabled
Info :  hart 2: currently disabled
Info :  hart 3: XLEN=64, misa=0x800000000014112d
Info :  hart 4: currently disabled
Info : datacount=2 progbufsize=16
Info : Disabling abstract command reads from CSRs.
Info : Core 4 could not be made part of halt group 1.
Info : Examined RISC-V core; found 5 harts
Info :  hart 0: currently disabled
Info :  hart 1: currently disabled
Info :  hart 2: currently disabled
Info :  hart 3: currently disabled
Info :  hart 4: XLEN=64, misa=0x800000000014112d
Info : Listening on port 3333 for gdb connections
Info : accepting 'gdb' connection on tcp/3333
Info : New GDB Connection: 1, Target mpfs.hart0_e51, state: halted
Info : Disabling abstract command writes to CSRs.
Info : Disabling abstract command writes to CSRs.
Info : Disabling abstract command writes to CSRs.
Info : Disabling abstract command writes to CSRs.
Info : Disabling abstract command writes to CSRs.
Info : dropped 'gdb' connection
Info : accepting 'gdb' connection on tcp/3333
Info : New GDB Connection: 1, Target mpfs.hart0_e51, state: halted
Info : dropped 'gdb' connection
Info : accepting 'gdb' connection on tcp/3333

```


$make clean lim
$make lim-debug   or make debug


Building for ddr

$make clean ddr
$make debug-ddr  

This will downlaod the propgram to the ddr memory and execute.  do not use monitor reset command.

$make attach-ddr

If the program is loaded by the bootloader into the ddr memroy and you want to debug that, use the "atatch-ddr" comamnd 

When doign the debug espwecially with ddr, make sure taht you do a clean power cycle of the board and the ddr trainign by the bootloader is complete.

```
C:\Microchip\SoftConsole-v2022.2-RISC-V-747\riscv-unknown-elf-gcc\bin\riscv64-unknown-elf-gdb.exe: warning: Couldn't determine a path for the index cache directory.
GNU gdb (xPack GNU RISC-V Embedded GCC (Microsemi SoftConsole build), 64-bit) 9.1
Copyright (C) 2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "--host=x86_64-w64-mingw32 --target=riscv64-unknown-elf".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<https://github.com/sifive/freedom-tools/issues>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from build/ddr/pfsoc_app.elf...
Remote debugging using localhost:3333
0x000000000a005000 in ?? ()
Loading section .text, size 0x2aa20 lma 0x80000000
Loading section .sdata, size 0x40 lma 0x8002aa20
Loading section .data, size 0xe30 lma 0x8002aa60
Start address 0x0000000080000000, load size 178320
Transfer rate: 13 KB/sec, 13716 bytes/write.

Thread 5 (Thread 5):
The riscv_frame_cache's start_addr is 0 (from the get_frame_func).
Forcing it to the value of the PC (0x000000000a004588) to avoid the riscv_scan_prologue reading between 0-99 addresses.

Thread 4 (Thread 4):
The riscv_frame_cache's start_addr is 0 (from the get_frame_func).
Forcing it to the value of the PC (0x000000000a0044e4) to avoid the riscv_scan_prologue reading between 0-99 addresses.

Thread 3 (Thread 3):
The riscv_frame_cache's start_addr is 0 (from the get_frame_func).
Forcing it to the value of the PC (0x000000000a004440) to avoid the riscv_scan_prologue reading between 0-99 addresses.

Thread 2 (Thread 2):
The riscv_frame_cache's start_addr is 0 (from the get_frame_func).
Forcing it to the value of the PC (0x000000000a00439c) to avoid the riscv_scan_prologue reading between 0-99 addresses.

Thread 1 (Thread 1):
Breakpoint 1 at 0x80002e30: file application/hart1/u54_1.c, line 707.
Continuing.

Thread 1 received signal SIGINT, Interrupt.
wait_main_hart () at platform/mpfs_hal/startup_gcc/mss_entry.S:460
--Type <RET> for more, q to quit, c to continue without paging--
460         csrr a2, mip
(gdb) c
Continuing.
[Switching to Thread 2]

Thread 2 hit Breakpoint 1, u54_1 () at application/hart1/u54_1.c:707
707         zero_bss_section();
(gdb) c
Continuing.

```

eth-disco-> help

================ Diagnostic & Test Suite CLI ================
  status                        - Display MAC, IP, link & packet counters
  stats                         - Display detailed MAC MIB statistics
  stats clear                   - Clear hardware MAC statistics
  ping <ip1>.<ip2>.<ip3>.<ip4>  - Send ICMP Echo Request to host IP
  promisc                       - Toggle GEM Promiscuous Mode
  set ip <ip1>.<ip2>.<ip3>.<ip4>- Manually change board IPv4 address
  set mac <XX:XX:XX:XX:XX:XX>   - Manually change hardware MAC address
  loopback pcs                  - Toggle GEM PCS SGMII Loopback
  loopback phy                  - Toggle VSC8221 PHY Loopback
  loopback off                  - Disable all loopback modes
  capture                       - Arm single-packet capture
  monitor                       - Toggle continuous packet monitoring
  rx inspect                    - Detailed layer-by-layer frame decoder
  rx flush                      - Clear and re-arm RX capture engine
  packet diff [bytes]           - Side-by-side TX vs RX buffer comparison
  desc inspect                  - Inspect DMA descriptor queue pointers
  mactest <len> <count>         - Run automated Layer 2 pattern test
  tx raw <len> [pat_hex]        - Transmit custom L2 raw frame
  tx arp                        - Transmit sample ARP frame
  tx udp <ip> <sp> <dp> <msg>   - Transmit raw IPv4 UDP packet
  tx tcp <ip> <sp> <dp> <flg> <m> - Transmit raw IPv4 TCP packet
  dhcp                          - Broadcast DHCP Discover packet
  speed <10|100|1000> <half|full> - Force MAC/PHY speed & duplex
  phy reset                     - Issue software reset to VSC8221 PHY
  mdio scan                     - Scan MDIO bus (0-31) for PHYs
  mdio read <reg_hex>           - Read 16-bit PHY register
  mdio write <r> <v>            - Write 16-bit PHY register
  phy dump                      - Dump PHY registers (0x00-0x0F)
  gem dump                      - Dump GEM0 MAC registers (0x000-0x084)
  help                          - Print Help Menu
===============================================================

eth-disco> status
[STATUS] MAC Addr: 00:FC:00:12:34:58 | IP Addr: 192.168.20.107
[STATUS] Link: UP | Speed: 1Gbps | Duplex: Full
[STATUS] Transmitted Packets: 23 | Received Packets: 3769
eth-disco>

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

eth-disco>