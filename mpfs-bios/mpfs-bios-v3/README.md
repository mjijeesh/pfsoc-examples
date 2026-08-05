# PolarFire SoC Baremetal BIOS v3


Added the command history for the BIOS. takes around 79KB total Now. without command history it was 75KB.
Changed the abnner to Poalrfire SoC.

Added hw_info command : read the ienabled peripherals and hw clock infor fro mthe xml file (python sscript generates the headers)

tested the memory read/write/comapre with lim and ddr memroy locations



## How to use this example


```
  ____  ___  _     _     ____  _____ ___ ____  _____   ____   ___   ____ 
 |  _ \/ _ \| |   / \   |  _ \|  ___|_ _|  _ \| ____| / ___| / _ \ / ___|
 | |_) | | | | |  / _ \  | |_) | |_   | || |_) |  _|   \___ \| | | | |    
 |  __/| |_| | |_/ ___ \ |  _ <|  _|  | ||  _ <| |___   ___) | |_| | |___
 |_|    \___/|____/_/   \_\|_| \_\_|   |___|_| \_____| |____/ \___/ \____|

  microchipPolarFire SoC BareMetalBios
  --------------------------------------------------
  hart0E51MonitorCore      : active
  harts1To4U54Application  : parkedInWfi
  memoryMap                : lim(0x08000000) | ddr(0x80000000)

==================================================
        PolarFire SoC Hardware Description        
==================================================
  [Design Information]
    Target Part       : MPFS095T (FCSG325)
    Design Name       : MPFS_DISCOVERY_KIT_MSS
    Libero Version    : 2023.1

  [System Clocks]
    CPU Coreplex Clock: 600 MHz (600000000 Hz)
    System Clock      : 600 MHz (600000000 Hz)
    AXI Bus Clock     : 300 MHz (300000000 Hz)
    APB/AHB Bus Clock : 150 MHz (150000000 Hz)
    DDR PHY Rate      : 1600 MT/s (1600000000 Hz)

  [Memory Map]
    Reset Vector      : 0x20220000 (Harts 0-4)
    LIM Memory        : 0x08000000 - 0x081FFFFF (2MB)
    DDR 32-bit Cache  : 0x80000000 - 0x800FFFFF (1MB)
    DDR 32-bit NonCache: 0xC0000000 - 0xC00FFFFF (1MB)
    DDR 64-bit Cache  : 0x1000000000 - 0x10000FFFFF
    DDR 64-bit NonCache: 0x1400000000 - 0x14000FFFFF

  [Configured MSS Peripherals]
    Active: SD/SDIO GEM0 SPI0 SPI1 MMUART0 MMUART1 MMUART4 I2C0 GPIO1 GPIO2 
==================================================

litex> ident
Ident: LiteX BIOS on Microchip PolarFire SoC (E51 Core)
litex> help

LiteX BIOS, available commands:

-- System Commands --
  help             - Print available commands
  ident            - Identifier of the system
  hw_info          - Display hardware map and peripherals

-- Boot Commands --
  boot             - Boot from Memory: boot <addr> [r1]
  serialboot       - Boot from Serial (SFL)

-- Memory Commands --
  mem_read         - Read memory: mem_read <addr> [len]
  mem_write        - Write memory: mem_write <addr> <val>
  mem_copy         - Copy memory: mem_copy <dst> <src> [n]
  mem_test         - Test memory access: mem_test <addr>
  mem_cmp          - Compare memory: mem_cmp <a1> <a2> <n>

litex> mem_read 0x80000000 32
Reading 1024 bytes from 0x0000000080000000:

0x0000000080000000: 00000000 00000000 deadbeef deadbeef 
0x0000000080000010: deadbeef deadbeef deadbeef deadbeef 
0x0000000080000020: deadbeef deadbeef deadbeef deadbeef 
0x0000000080000030: deadbeef deadbeef deadbeef deadbeef 
0x0000000080000040: 00000000 00000000 deadbeef deadbeef 


litex> mem_write 0x80000000 164
Wrote 0x00000400 to 0x0000000080000000
litex> mem_read 0x80000000 32   
Reading 32 bytes from 0x0000000080000000:

0x0000000080000000: 00000400 00000000 deadbeef deadbeef 
0x0000000080000010: deadbeef deadbeef deadbeef deadbeef 

litex> mem_write
Usage: mem_write <address> <value>
litex> mem_copy
Usage: mem_copy <dst> <src> [count (32-bit words)]
litex> mem_copy 0x80000000 0x08000000 1024
Copied 1024 words from 0x8000000 to 0x80000000
litex> mem_read 0x80000000 128
Reading 128 bytes from 0x0000000080000000:

0x0000000080000000: 51c000ef 00000717 2b470713 30571073 
0x0000000080000010: 305027f3 fef71ee3 00800613 30063073 
0x0000000080000020: 30401073 34401073 f1402573 00050663 
0x0000000080000030: 30305073 30205073 34001073 34201073 
0x0000000080000040: 34101073 3a001073 3a201073 00000093 
0x0000000080000050: 00000113 00000193 00000213 00000293 
0x0000000080000060: 00000313 00000393 00000413 00000493 
0x0000000080000070: 00000513 00000593 00000613 00000693 

litex> mem_cmp
Usage: mem_cmp <addr1> <addr2> <count_words>
litex> mem_cmp 0x8000000 0x80000000 128
Memory region contents are IDENTICAL.
litex> mem_cmp 0x8000000 0x80000000 256
Memory region contents are IDENTICAL.
litex> mem_cmp 0x8000000 0x80000000 2048
Mismatch at word 1024: [0x0x8001000]=0xd79bfcc4 vs [0x0x80001000]=0x00000000


```


### DDR training and Renode
If the firmware has DDR training enabled, then the application will take significantly longer to start up in Renode. Training has no practical impact in this environment as the emulated DDR memory is already reliable.

Training can be controlled by removing `#define DDR_SUPPORT` in your `mss_sw_config.h` file. This change should be made in `src\boards\[BOARD]\platform_config\[BUILD-CONFIGURATION]\mpfs_hal_config\`

If your project uses the default configuration file in `src\platform\platform_config_reference\` to enable DDR training, it is recommended to create a copy under the boards directory and disable `DDR_SUPPORT` there.
