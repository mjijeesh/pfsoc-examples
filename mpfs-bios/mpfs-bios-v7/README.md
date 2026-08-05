# PolarFire SoC Baremetal BIOS v7



---------------------------------------------------------------------------------------------------------------------------------
##### What is new V7

1. Parallel runnign of bios on e51 and the new app on u54_1 core.
2. initially bios will run from e51 , and via serialboot vommand, the atrget application will be loaded o nto the memroy.
3. The the new applicatio nwill be executed fro mu54_1, while the bios still runnign from e51.

MMUART1 is used for the target_app and MMUART0 is used for BIOS.


On the Target app's Makefile

# Route standard newlib printf() to U54 MMUART1
`CFLAGS += -DMICROCHIP_STDIO_THRU_MMUARTX=\&g_mss_uart1_lo`


On BIOS app Makefile


# Route standard newlib printf() to E51 MMUART0                                                                                                                                         
`CFLAGS += -DMICROCHIP_STDIO_THRU_MMUARTX=\&g_mss_uart0_lo`

Simialrly teh peripheral_clk and uart init for the two apps needs to eb taken care.

Tested and working.




---------------------------------------------------------------------------------------------------------------------------------
##### What is new V6

1. Fixed the halti hart booting, now all the harts enabled, serialboot is working.

On the boot command, below modificatiosn are done.


```
// Shared jump address published to secondary harts
volatile uint64_t g_app_jump_addr = 0;

typedef void (*entry_func_t)(unsigned long r1, unsigned long r2, unsigned long r3);

void __attribute__((noreturn)) boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr)
{
    printf("Booting all harts at 0x%016lx...\n\n", addr);
    uart_sync();

    // 1. Publish target address to memory
    g_app_jump_addr = addr;
    asm volatile("fence" ::: "memory");

    // 2. Trigger IPIs to wake Harts 1 through 4 from BIOS wfi
    for (uint32_t hart_id = 1; hart_id <= 4; hart_id++) {
        raise_soft_interrupt(hart_id);
    }

    // 3. Delay to allow secondary harts to unblock and read g_app_jump_addr
    for (volatile int i = 0; i < 100000; i++);

    // 4. Memory barriers and interrupt disable on Hart 0
    __disable_irq();
    asm volatile("fence.i" ::: "memory");

    // 5. Jump Hart 0 to application payload entry point
    entry_func_t entry = (entry_func_t)(uintptr_t)addr;
    entry(r1, r2, r3);

    while (1);
}
```



### 2. BIOS Secondary Hart Entry (`application/hart1/u54_1.c`)
*(Repeat for `u54_2.c`, `u54_3.c`, and `u54_4.c` in the BIOS project)*[cite: 11, 12, 13, 14]

On the other harts, instead of usign the boot entry inside the software_irq handler, now modev inside the mmain program itself.
the jumping to the new adress will happen if the global adress variabel is changed .

When the booting happens, the interrupts are disabled, and noen of the software interrupt handles will work,
but the wfi will wake up the harts fro msleep mode even when the global interrupt is disavled.
This will force the hart to execute the next  instruction instead of executing the handler.

with all the harts enabled, the monitpr core will need to get synchronizatio nfro mal lthe the other harts otherwise the program will be hanging i na deadlock.




```
#include "mpfs_hal/mss_hal.h"

extern volatile uint64_t g_app_jump_addr;

typedef void (*entry_func_t)(unsigned long r1, unsigned long r2, unsigned long r3);

void u54_1(void) 
{
    clear_soft_interrupt();
    set_csr(mie, MIP_MSIP);

    while (1) {
        // Wait for IPI from Hart 0
        __asm__ volatile ("wfi");

        // Unblock check for published target address
        if (g_app_jump_addr != 0) {
            clear_soft_interrupt();
            entry_func_t entry = (entry_func_t)(uintptr_t)g_app_jump_addr;

            asm volatile("fence" ::: "memory");
            asm volatile("fence.i" ::: "memory");

            // Jump Hart 1 directly to target application startup (_start)
            entry(0, 0, 0);
        }
    }
}

void Software_h1_IRQHandler(void)
{
    clear_soft_interrupt();
}
```

On the target application , following modificatiosn are made, 

---

### 3. Target Application `mss_sw_config.h`
```c
#ifndef MSS_SW_CONFIG_H_
#define MSS_SW_CONFIG_H_

// Enable startup across all 5 harts
#ifndef MPFS_HAL_FIRST_HART
#define MPFS_HAL_FIRST_HART    0
#endif

#ifndef MPFS_HAL_LAST_HART
#define MPFS_HAL_LAST_HART     4
#endif

#define IMAGE_LOADED_BY_BOOTLOADER 1

// DO NOT clear memory on startup (prevents self-erasure of RAM image)
#ifndef MPFS_HAL_CLEAR_MEMORY
#define MPFS_HAL_CLEAR_MEMORY  0
#endif

#define MSSIO_SUPPORT

#endif /* MSS_SW_CONFIG_H_ */
```

Also on the Makefile of the target applciation, enables all the harts 
---

### 4. Target Application `Makefile`
```makefile
else ifeq ($(MEM_TARGET),lim-app)
    BUILD_DIR     := build/lim-app
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/lim-release/linker/mpfs-lim-app.ld
    CONFIG_DIR    := lim-release
    CFLAGS        += -DIMAGE_LOADED_BY_BOOTLOADER=1
    CFLAGS        += -DMPFS_HAL_CLEAR_MEMORY=0
    CFLAGS        += -DMPFS_HAL_FIRST_HART=0 -DMPFS_HAL_LAST_HART=4
```

---------------------------------------------------------------------------------------------------------------------------------
##### What is new V5

1. Added the serialboot command. and its booting the application
2. Booting of applicatio nfrom lim is tested.
3. The bios application is buidl usign 256KB of lim memory.
4. demo-app is build usign the upper 256KB of the lim memory. This way there is no conflict of memory usages.

5. modified the   MPFS_HAL_FIRST_HART and  MPFS_HAL_LAST_HART so that only 1 hart is used i nthe target app.

6. This is doen because the bios keeps al lthe other harts in `efi` mode, when the new app is booting the app wait


On boot, system_startup.c on Hart 0 (E51) attempts to synchronize all harts by polling their Hart Local Storage (HLS) magic registers. 
Because the BIOS parked Harts 1–4 in wfi, they never responded, causing Hart 0 to loop indefinitely.  
#### Solution: 
  Added application-level preprocessor flags -DMPFS_HAL_FIRST_HART=0 and -DMPFS_HAL_LAST_HART=0. 
  This restricts the HAL startup sequence solely to Hart 0 (E51), bypassing the multi-hart synchronization loop.   or

  change the settings in the  file  boards/mpfs-discovery-kit/platform_config/lim-release/mpfs_hal_config/mss_sw_config.h

  ```
    // Restrict HAL startup synchronization strictly to Hart 0 (E51)
  #ifndef MPFS_HAL_FIRST_HART
  #define MPFS_HAL_FIRST_HART    0
  #endif

  #ifndef MPFS_HAL_LAST_HART
  #define MPFS_HAL_LAST_HART     0
  #endif

  ```


### Complete Build and Execution Workflow

Build the BIOS Bootloader (Mapped to 0x08000000):

`make clean lim BOARD=mpfs-discovery-kit`

Build the Secondary Application (Mapped to 0x08040000):
This is under the demo-app folder 
  

`make lim-app BOARD=mpfs-discovery-kit`

Launch the Terminal & Stream Payload:

`litex_term --serialboot --kernel build/lim-app/pfsoc_app.bin --kernel-adr 0x08040000 /dev/ttyUSB0`

Initiate Boot:
    In the serial terminal, issue the serialboot command:
`litex> serialboot`



---------------------------------------------------------------------------------------------------------------------------------
##### What is new V4

1. Added the serialboot command.
2. Works with the original litex_term. uploading of the demo.bin fiel is working.
3. use the litex_flash_termp.py to just upload without booting, use --no-boot command option. 

----------------------------------------------------------------------------------------------------------------------------------
##### What is new V3
1. Added hw_info command : read the ienabled peripherals and hw clock infor fro mthe xml file (python sscript generates the headers)

2. tested the memory read/write/comapre with lim and ddr memroy locations
----------------------------------------------------------------------------------
#### What is New V2:
1. Added the command history for the BIOS. takes around 79KB total Now. without command history it was 75KB.
2. Changed the abnner to Poalrfire SoC.
-------------------------------------------------------------------------------------



## How to use this example v4


1. With the original `litex_term.py`  application 
```
(litex-env) jijeesh@jijeesh-Latitude-5300:~/git/pfsoc-examples/mpfs-bios/mpfs-bios-v4$ litex_term /dev/ttyUSB3  --kernel build/lim/pfsoc_app.bin --kernel-adr 0x80000000
help

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

litex> serialboot
Booting from serial interface (SFL)...
Press Q or ESC to abort serial boot.
sL5DdSMmkekro
[LITEX-TERM] Received firmware download request from the device.
[LITEX-TERM] Uploading build/lim/pfsoc_app.bin to 0x80000000 (86784 bytes)...
[LITEX-TERM] Upload calibration... (inter-frame:  0.00us, length: 251, window: 8)
[LITEX-TERM] Upload complete (10.9KB/s).
[LITEX-TERM] Booting the device.
[LITEX-TERM] Done.
Executing application at 0x0000000080000000...

```

The application hangs becuase the binary is not made to run from the DDR location 0x80000000 , this is expected.

2. Now with custom `litex_flash_term.py` . This has a --no-boot option, which dont send the boot command. So after uplaodign the file, it just exit to the bios menu.

```
(litex-env) jijeesh@jijeesh-Latitude-5300:~/git/pfsoc-examples/mpfs-bios/mpfs-bios-v4$ ./litex_flash_term.py /dev/ttyUSB3  --no-boot --kernel build/lim/pfsoc_app.bin --kernel-adr 0x80000000

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

litex> serialboot
Booting from serial interface (SFL)...
Press Q or ESC to abort serial boot.
sL5DdSMmkekro
[LITEX-TERM] Received firmware download request from the device.
[LITEX-TERM] Uploading build/lim/pfsoc_app.bin to 0x80000000 (86784 bytes)...
[LITEX-TERM] Upload calibration... (inter-frame:  0.00us, length: 251, window: 8)
[LITEX-TERM] Upload complete (10.9KB/s).
[LITEX-TERM] RAM Download complete. Returning to prompt (--no-boot).
[LITEX-TERM] Aborting serial boot.
[LITEX-TERM] Serial boot aborted.
litex> mem_read 0x80000000 2048
Reading 2048 bytes from 0x0000000080000000:

0x0000000080000000: 51c000ef 00000717 2b470713 30571073 
0x0000000080000010: 305027f3 fef71ee3 00800613 30063073 
0x0000000080000020: 30401073 34401073 f1402573 00050663 
0x0000000080000030: 30305073 30205073 34001073 34201073 
0x0000000080000040: 34101073 3a001073 3a201073 00000093 
0x0000000080000050: 00000113 00000193 00000213 00000293 
0x0000000080000060: 00000313 00000393 00000413 00000493 
0x0000000080000070: 00000513 00000593 00000613 00000693 
0x0000000080000080: 00000713 00000793 00000813 00000893 


```





## How to use this example v3


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
