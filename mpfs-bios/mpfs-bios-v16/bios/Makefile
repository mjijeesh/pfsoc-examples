# ==============================================================================
# Advanced Multi-Directory PolarFire SoC Makefile
# Configured for LiteX BIOS / Bare-Metal Port
# Supports Memory Targets: eNVM (XIP), LIM, LIM-APP, DDR, ENVM-LIM
# ==============================================================================

# Automatically uses riscv64-unknown-elf- or riscv-none-elf- based on system PATH
CROSS_COMPILE ?= riscv-none-elf-

CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)as
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size
GDB     := $(CROSS_COMPILE)gdb

TARGET  := pfsoc_bios

# Capture current build timestamp from host system
BUILD_TIMESTAMP := $(shell date +"%Y-%m-%d %H:%M:%S")
CFLAGS += -DBUILD_TIMESTAMP="\"$(BUILD_TIMESTAMP)\""


# To use FPGA CoreSPI IP:
#CFLAGS += -DUSE_CORE_SPI
# OR to use Hard MSS SPI:
# CFLAGS += -DUSE_MSS_SPI


ifeq ($(BOARD),mpfs-icicle-kit)
    CFLAGS += -DUSE_CORE_SPI -DCORESPI_BASE_ADDR=0x40000000UL
else
    CFLAGS += -DUSE_MSS_SPI
endif







# ==============================================================================
# OS Detection & OpenOCD Executable Path Configuration
# ==============================================================================
ifeq ($(OS),Windows_NT)
    OPENOCD ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/openocd/bin/openocd.exe
else
    OPENOCD ?= /opt/microchip/SoftConsole/SoftConsole-v2022/openocd/bin/openocd
endif

# ------------------------------------------------------------------------------
# Dynamic Board Configuration & Validation
# ------------------------------------------------------------------------------
BOARD     ?= mpfs-discovery-kit
BOARD_DIR ?= boards/$(BOARD)

AVAILABLE_BOARDS := $(notdir $(wildcard boards/*))

# ==============================================================================                                                                                                        
# SECTION 2: BOARD & DEVICE (DIE / PACKAGE) SELECTION                                                                                                                                   
# ==============================================================================                                                                                                        

# Default Board Selection (Can be overridden from CLI: make BOARD=mpfs-icicle-kit)                                                                                                      
BOARD     ?= mpfs-discovery-kit
BOARD_DIR := boards/$(BOARD)

# Discover Available Board Configurations Under boards/ Directory                                                                                                                       
AVAILABLE_BOARDS := $(notdir $(wildcard boards/*))

# Automatically Assign Default FPGA Device Parameters Based on Target Board                                                                                                             
# (Can be overridden from CLI: make DIE=MPFS250T PACKAGE=FCG1152)                                                                                                                       
ifeq ($(BOARD),mpfs-icicle-kit)
    DIE_DEFAULT     := MPFS250T
	PACKAGE_DEFAULT := FCVG484
    
else ifeq ($(BOARD),mpfs-video-kit)
    DIE_DEFAULT     := MPFS250T
	PACKAGE_DEFAULT := FCG1152
    
else
    # Default for mpfs-discovery-kit and standard custom boards                                                                                                                         
    DIE_DEFAULT     := MPFS095T
    PACKAGE_DEFAULT := FCSG325
endif

DIE     ?= $(DIE_DEFAULT)
PACKAGE ?= $(PACKAGE_DEFAULT)

# Add near the CFLAGS configuration section
CFLAGS += -DMPFS_TARGET_DIE="\"$(DIE)\"" -DMPFS_TARGET_PACKAGE="\"$(PACKAGE)\""





ifeq ($(wildcard $(BOARD_DIR)),)
    $(info ========================================================================)
    $(info [ERROR] Board directory '$(BOARD_DIR)' (BOARD=$(BOARD)) does not exist!)
    $(info ========================================================================)
    $(info Available boards under 'boards/':)
    $(foreach b,$(AVAILABLE_BOARDS),$(info   * $b))
    $(info ========================================================================)
    $(error Build aborted due to invalid board selection)
endif

# ------------------------------------------------------------------------------
# SoC Header Generator Tooling Configuration
# ------------------------------------------------------------------------------
PYTHON         ?= python3
SOC_GEN_SCRIPT := platform/soc_config_generator/mpfs_configuration_generator.py
XML_CONFIG_DIR ?= $(BOARD_DIR)/fpga_design/design_description
CONFIG_OUT_DIR ?= $(BOARD_DIR)

BOARD_XML_FILES := $(wildcard $(XML_CONFIG_DIR)/*.xml)

ifeq ($(BOARD_XML_FILES),)
    $(info ========================================================================)
    $(info [ERROR] No XML hardware description file (.xml) found!)
    $(info Target Directory : $(XML_CONFIG_DIR))
    $(info Selected Board   : $(BOARD) ($(BOARD_DIR)))
    $(info ========================================================================)
    $(error Build aborted due to missing XML configuration file)
endif


# Maps BOARD variable to compiler preprocessor macro for uart_mapping.c                                                                                                                 
ifeq ($(BOARD),mpfs-discovery-kit)
    #CFLAGS += -DMPFS_DISCOVERY_KIT
	# Default UART Mapping for E51 Monitor Core
	CFLAGS += -DMICROCHIP_STDIO_THRU_MMUARTX=\&g_mss_uart0_lo
else ifeq ($(BOARD),mpfs-icicle-kit)
    #CFLAGS += -DMPFS_ICICLE_KIT_ES
	# Default UART Mapping for E51 Monitor Core
	CFLAGS += -DMICROCHIP_STDIO_THRU_MMUARTX=\&g_mss_uart0_lo
else ifeq ($(BOARD),mpfs-video-kit)
    CFLAGS += -DMPFS_VIDEO_KIT
	# Default UART Mapping for E51 Monitor Core
	CFLAGS += -DMICROCHIP_STDIO_THRU_MMUARTX=\&g_mss_uart0_lo
else
    BOARD_MACRO := $(shell echo $(BOARD) | tr 'a-z-' 'A-Z_')
    CFLAGS += -D$(BOARD_MACRO)
endif


# ------------------------------------------------------------------------------
# Debugger Configuration
# ------------------------------------------------------------------------------
OPENOCD_CFG  ?= -c "set DEVICE MPFS" -f board/microsemi-riscv.cfg
GDB_PORT     ?= 3333

# ------------------------------------------------------------------------------
# Dynamic Memory Target Configuration Switcher
# ------------------------------------------------------------------------------
MEM_TARGET ?= lim

ifeq ($(MEM_TARGET),envm)
    # Direct Execute-in-Place (XIP) from eNVM
    BUILD_DIR     := build/envm
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/envm-release/linker/mpfs-envm.ld
    CONFIG_DIR    := envm-release
else ifeq ($(MEM_TARGET),ddr)
    BUILD_DIR     := build/ddr
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/ddr-release/linker/mpfs-ddr-loaded-by-boot-loader.ld
    CONFIG_DIR    := ddr-release
    CFLAGS        += -DIMAGE_LOADED_BY_BOOTLOADER=1
else ifeq ($(MEM_TARGET),lim-app)
    BUILD_DIR     := build/lim-app
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/lim-release/linker/mpfs-lim-app.ld
    CONFIG_DIR    := lim-release
    CFLAGS        += -DIMAGE_LOADED_BY_BOOTLOADER=1
else ifeq ($(MEM_TARGET),envm-lim)
    BUILD_DIR     := build/envm-lim
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/envm-scratchpad-release/linker/mpfs-envm-lma-scratchpad-vma.ld
    CONFIG_DIR    := envm-scratchpad-release
else
    # Defaulting to LIM
    BUILD_DIR     := build/lim
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/lim-debug/linker/mpfs-lim.ld
    CONFIG_DIR    := lim-debug
endif

# Ensure sub-makes retain MEM_TARGET context
export MEM_TARGET

# ------------------------------------------------------------------------------
# Core Architecture Flags
# ------------------------------------------------------------------------------
ARCH_FLAGS := -march=rv64gc \
              -mabi=lp64d \
              -mcmodel=medany \
              -msmall-data-limit=8 \
              -mstrict-align \
              -mno-save-restore

# ------------------------------------------------------------------------------
# Source Code & Include Directory Discovery
# ------------------------------------------------------------------------------
DRIVER_DIRS := $(wildcard platform/drivers/*)
DRIVER_DIRS += platform/drivers/mss/mss_mmuart
DRIVER_DIRS += platform/drivers/mss/mss_gpio
DRIVER_DIRS += platform/drivers/mss/mss_spi

# ------------------------------------------------------------------------------
# Source Code & Include Directory FPGA IPs
# ------------------------------------------------------------------------------
DRIVER_DIRS += platform/drivers/fpga_ip/CoreSPI

INCLUDE_DIRS += $(BOARD_DIR)/platform_config/$(CONFIG_DIR) \
                $(BOARD_DIR)/platform_config/$(CONFIG_DIR)/mpfs_hal_config \
                application \
                platform \
                $(BOARD_DIR) \
                $(DRIVER_DIRS)

INC_FLAGS := $(addprefix -I, $(INCLUDE_DIRS))

# Discover C and Assembly sources
RAW_SRCS_C := $(wildcard application/*.c) \
              $(wildcard application/*/*.c) \
              $(wildcard platform/drivers/*/*.c) \
              $(wildcard platform/mpfs_hal/startup_gcc/*.c) \
              $(wildcard platform/mpfs_hal/common/*.c) \
              $(wildcard platform/mpfs_hal/common/nwc/*.c) \
              $(wildcard platform/hal/*.c) \
              platform/drivers/mss/mss_mmuart/mss_uart.c \
              platform/drivers/mss/mss_gpio/mss_gpio.c \
			  platform/drivers/mss/mss_spi/mss_spi.c \
			  platform/drivers/fpga_ip/CoreSPI/core_spi.c


SRCS_C := $(sort $(RAW_SRCS_C))

SRCS_ASM := $(wildcard platform/hal/*.S) \
            $(wildcard platform/mpfs_hal/startup_gcc/*.S)

OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS_C)) \
        $(patsubst %.S, $(BUILD_DIR)/%.o, $(SRCS_ASM))

# ------------------------------------------------------------------------------
# Compilation & Linker Flags
# ------------------------------------------------------------------------------
CFLAGS += $(ARCH_FLAGS) \
          -O0 \
          -g3 \
          -fmessage-length=0 \
          -fsigned-char \
          -ffunction-sections \
          -fdata-sections \
          -std=gnu11 \
          -Wstrict-prototypes \
          -Wbad-function-cast \
          --specs=nano.specs         
          
CFLAGS += $(INC_FLAGS)

LDFLAGS := $(ARCH_FLAGS) \
           -O0 \
           -g3 \
           -nostartfiles \
           -T $(LINKER_SCRIPT) \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
           -Wl,--gc-sections \
           --specs=nano.specs \
           --specs=nosys.specs \
           -Wl,--no-warn-rwx-segments

# ==============================================================================
# eNVM Boot Mode Programmer & Hardware Tooling Configuration
# ==============================================================================
export SC_INSTALL_DIR ?= /opt/microchip/SoftConsole/SoftConsole-v2022
export FPGENPROG      ?= /opt/microchip/Libero_SoC_2025.2/Libero_SoC/Designer/bin64/fpgenprog

BOOTMODE_PROGRAMMER   ?= $(SC_INSTALL_DIR)/extras/mpfs/mpfsBootmodeProgrammer.jar

# Target FPGA Hardware Parameters (MPFS Discovery Kit)
DIE     ?= MPFS095T
PACKAGE ?= FCSG325

# ==============================================================================
# Core Automation Targets
# ==============================================================================

.PHONY: all clean show_size lim lim-app envm envm-lim ddr generate_headers openocd debug gdbinit help program-envm program-envm-lim flash-envm

help:
	@echo "========================================================================"
	@echo " PolarFire SoC Bare-Metal Build System"
	@echo " Active Board Path : $(BOARD_DIR)"
	@echo " Available Boards  : $(AVAILABLE_BOARDS)"
	@echo "========================================================================"
	@echo " Usage: make <target> [BOARD=<board_folder_name>]"
	@echo ""
	@echo " Primary Build Targets:"
	@echo "   envm              - Build for direct Execute-In-Place (XIP) from eNVM"
	@echo "   lim               - Build BIOS for lower LIM (0x08000000)"
	@echo "   lim-app           - Build Demo App for high LIM (0x08040000)"
	@echo "   ddr               - Build application for volatile DDR Memory"
	@echo "   envm-lim          - Build application for eNVM -> LIM scratchpad copy"
	@echo "   all               - Build executable images for active MEM_TARGET"
	@echo ""
	@echo " Flashing & Hardware Debugging:"
	@echo "   program-envm      - Flash eNVM XIP build into eNVM (Boot Mode 1)"
	@echo "   program-envm-lim  - Flash eNVM-LIM build into eNVM (Boot Mode 1)"
	@echo "   openocd           - Launch OpenOCD JTAG connection server"
	@echo "   debug             - Launch GDB session and connect to running OpenOCD server"
	@echo "========================================================================"

envm:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=envm all

lim-app:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=lim-app all
	
lim:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=lim all

envm-lim:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=envm-lim all

ddr:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=ddr all

generate_headers:
	@echo "=================================================="
	@echo " Generating SoC Configuration Headers from XML"
	@echo " Target Board: $(BOARD_DIR)"
	@echo "=================================================="
	@$(PYTHON) $(SOC_GEN_SCRIPT) $(XML_CONFIG_DIR) $(CONFIG_OUT_DIR)

all: generate_headers $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex show_size

$(OBJS): | generate_headers

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@echo " [LD]  Linking payload ($@) using script: $(LINKER_SCRIPT)"
	@$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo " [CC]  Compiling module: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo " [AS]  Assembling module: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@echo " [OBJ] Extracting raw application image: $@"
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	@echo " [OBJ] Compiling target flash image: $@"
	@$(OBJCOPY) -O ihex $< $@

show_size: $(BUILD_DIR)/$(TARGET).elf
	@echo ""
	@echo "=== Application Section Allocation Projections [$(MEM_TARGET)] ==="
	@echo " Active Linker Script : $(LINKER_SCRIPT)"
	@$(SIZE) $<
	@echo "=================================================="

clean:
	@echo " Removing all execution build trees..."
	@rm -rf build .gdbinit

# ------------------------------------------------------------------------------
# Flashing Targets
# ------------------------------------------------------------------------------

program-envm:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=envm all
	@echo "=================================================="
	@echo " Programming eNVM Direct XIP (Boot Mode 1)"
	@echo " Target Device : $(DIE) ($(PACKAGE))"
	@echo " Working Dir   : $(CURDIR)/build/envm"
	@echo "=================================================="
	java -jar $(BOOTMODE_PROGRAMMER) \
		--bootmode 1 \
		--die $(DIE) \
		--package $(PACKAGE) \
		--workdir $(CURDIR)/build/envm

program-envm-lim:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=envm-lim all
	@echo "=================================================="
	@echo " Programming eNVM-LIM Scratchpad (Boot Mode 1)"
	@echo " Target Device : $(DIE) ($(PACKAGE))"
	@echo " Working Dir   : $(CURDIR)/build/envm-lim"
	@echo "=================================================="
	java -jar $(BOOTMODE_PROGRAMMER) \
		--bootmode 1 \
		--die $(DIE) \
		--package $(PACKAGE) \
		--workdir $(CURDIR)/build/envm-lim

flash-envm:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=envm all
	@echo "=================================================="
	@echo " Flashing eNVM Image via OpenOCD/GDB Batch Session"
	@echo " Target Image: build/envm/$(TARGET).elf"
	@echo "=================================================="
	$(GDB) build/envm/$(TARGET).elf \
		-ex 'set $$target_riscv=1' \
		-ex 'set mem inaccessible-by-default off' \
		-ex 'target extended-remote localhost:$(GDB_PORT)' \
		-ex 'monitor reset halt' \
		-ex 'load' \
		-ex 'detach' \
		-ex 'quit'

# ------------------------------------------------------------------------------
# Debugging Automation Targets
# ------------------------------------------------------------------------------

openocd:
	@echo "=================================================="
	@echo " Launching OpenOCD Server for PolarFire SoC..."
	@echo " Press Ctrl+C to terminate connection server."
	@echo "=================================================="
	$(OPENOCD) $(OPENOCD_CFG)

debug:
	@ELF_PATH="$(BUILD_DIR)/$(TARGET).elf"; \
	if [ ! -f "$$ELF_PATH" ]; then \
		echo "ERROR: Executable not found at $$ELF_PATH. Run 'make $(MEM_TARGET)' first."; \
		exit 1; \
	fi; \
	echo "=================================================="; \
	echo " Starting GDB Session for: $$ELF_PATH"; \
	echo " Target Architecture Sync context: $(MEM_TARGET)"; \
	echo " Active Linker Script: $(LINKER_SCRIPT)"; \
	echo "=================================================="; \
	$(GDB) $$ELF_PATH \
		-ex 'set $$target_riscv=1' \
		-ex 'set mem inaccessible-by-default off' \
		-ex 'target extended-remote localhost:$(GDB_PORT)' \
		-ex 'monitor reset halt' \
		-ex 'load' \
		-ex 'thread apply all set $$pc = _start' \
		-ex 'continue'

gdbinit:
	@echo "Creating local .gdbinit..."
	@echo "set \$$target_riscv=1" > .gdbinit
	@echo "set mem inaccessible-by-default off" >> .gdbinit
	@echo "target extended-remote localhost:$(GDB_PORT)" >> .gdbinit
	@echo "file $(BUILD_DIR)/$(TARGET).elf" >> .gdbinit
	@echo ".gdbinit created for $(BUILD_DIR)/$(TARGET).elf."