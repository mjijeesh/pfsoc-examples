# ==============================================================================
# Advanced Multi-Directory PolarFire SoC Makefile
# Supports Memory Targets: eNVM, LIM, DDR
# ==============================================================================

CROSS_COMPILE ?= riscv-none-elf-

CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)as
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size
GDB     := $(CROSS_COMPILE)gdb

# Explicit absolute path to SoftConsole OpenOCD binary to avoid PATH collisions
OPENOCD ?= /opt/microchip/SoftConsole/SoftConsole-v2022/openocd/bin/openocd

TARGET    := pfsoc_app

# ------------------------------------------------------------------------------
# Dynamic Board Configuration & Validation
# ------------------------------------------------------------------------------
BOARD     ?= mpfs-discovery-kit
BOARD_DIR ?= boards/$(BOARD)

# Discovers available board subdirectories inside 'boards/'
AVAILABLE_BOARDS := $(notdir $(wildcard boards/*))

# 1. Stop execution if the specified BOARD_DIR directory does not exist
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

# Discover XML configuration files inside the board's XML directory
BOARD_XML_FILES := $(wildcard $(XML_CONFIG_DIR)/*.xml)

# 2. Stop execution if no .xml file is present for the board
ifeq ($(BOARD_XML_FILES),)
    $(info ========================================================================)
    $(info [ERROR] No XML hardware description file (.xml) found!)
    $(info Target Directory : $(XML_CONFIG_DIR))
    $(info Selected Board   : $(BOARD) ($(BOARD_DIR)))
    $(info ========================================================================)
    $(info Please place your Libero generated '*_mss_cfg.xml' or hardware XML)
    $(info description file inside: $(XML_CONFIG_DIR)/)
    $(info ========================================================================)
    $(error Build aborted due to missing XML configuration file)
endif

# ------------------------------------------------------------------------------
# SoC Header Generator Tooling Configuration
# ------------------------------------------------------------------------------
PYTHON         ?= python3
SOC_GEN_SCRIPT := platform/soc_config_generator/mpfs_configuration_generator.py
XML_CONFIG_DIR ?= $(BOARD_DIR)/fpga_design/design_description
CONFIG_OUT_DIR ?= $(BOARD_DIR)

# ------------------------------------------------------------------------------
# Debugger Configuration
# ------------------------------------------------------------------------------
OPENOCD_CFG  ?= -c " set DEVICE MPFS" -f board/microsemi-riscv.cfg
GDB_PORT     ?= 3333

# ------------------------------------------------------------------------------
# 1. Dynamic Memory Target Configuration Switcher
# ------------------------------------------------------------------------------
MEM_TARGET ?= lim

ifeq ($(MEM_TARGET),envm)
    BUILD_DIR     := build/envm
    LINKER_SCRIPT := linker/mpfs-envm.ld
    CONFIG_DIR    := envm-release
else ifeq ($(MEM_TARGET),ddr)
    BUILD_DIR     := build/ddr
    LINKER_SCRIPT := linker/mpfs-ddr.ld
    CONFIG_DIR    := ddr-release
else ifeq ($(MEM_TARGET),envm-lim)
    BUILD_DIR     := build/ddr
    LINKER_SCRIPT := linker/mpfs-envm-scratchpad.ld
    CONFIG_DIR    := envm-release
else
    # Defaulting to LIM
    BUILD_DIR     := build/lim
    LINKER_SCRIPT := linker/mpfs-lim.ld
    CONFIG_DIR    := lim-debug
endif

# ------------------------------------------------------------------------------
# 2. Core Target Architecture Flags
# ------------------------------------------------------------------------------
ARCH_FLAGS := -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany

# ------------------------------------------------------------------------------
# 3. Dynamic Source Code and Include Directory Discovery
# ------------------------------------------------------------------------------
DRIVER_DIRS := $(wildcard platform/drivers/*)

INCLUDE_DIRS += $(BOARD_DIR)/platform_config/$(CONFIG_DIR)
INCLUDE_DIRS += application \
                platform \
                $(BOARD_DIR) \
                $(DRIVER_DIRS)

INC_FLAGS := $(addprefix -I, $(INCLUDE_DIRS))

SRCS_C   := $(wildcard application/src/*.c) \
            $(wildcard platform/drivers/*/*.c)\
            $(wildcard platform/mpfs_hal/startup_gcc/*.c)\
            $(wildcard platform/mpfs_hal/common/*.c)\
            $(wildcard platform/mpfs_hal/common/nwc/*.c)\
            $(wildcard platform/hal/*.c)

SRCS_ASM := $(wildcard platform/hal/*.S)\
            $(wildcard platform/mpfs_hal/startup_gcc/*.S)

OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS_C)) \
        $(patsubst %.S, $(BUILD_DIR)/%.o, $(SRCS_ASM))

# ------------------------------------------------------------------------------
# 4. Compilation Flag Suites
# ------------------------------------------------------------------------------
#CFLAGS := $(ARCH_FLAGS) \
#          -Wall -Wextra -O2 \
#          -ffreestanding \
#          -ffunction-sections -fdata-sections \
#          $(INC_FLAGS)
# ------------------------------------------------------------------------------
# 4. Compilation Flag Suites
# ------------------------------------------------------------------------------
CFLAGS := $(ARCH_FLAGS) \
          -Wall -Wextra -O2 \
          -g \
          -ffreestanding \
          -ffunction-sections -fdata-sections \
          $(INC_FLAGS)
          
          
LDFLAGS = $(ARCH_FLAGS) \
          -nostartfiles \
          -Wl,--gc-sections \
          -T $(LINKER_SCRIPT) \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
          -Wl,--no-warn-rwx-segments

# ==============================================================================
# Core Automation Targets
# ==============================================================================

.PHONY: all clean show_size lim envm envm-lim ddr generate_headers openocd debug help

help:
	@echo "========================================================================"
	@echo " PolarFire SoC Bare-Metal Build System"
	@echo " Active Board Path : $(BOARD_DIR)"
	@echo " Available Boards  : $(AVAILABLE_BOARDS)"
	@echo "========================================================================"
	@echo " Usage: make <target> [BOARD=<board_folder_name>] [MEM_TARGET=lim|envm|envm-lim|ddr]"
	@echo "   or:  make <target> [BOARD_DIR=path/to/board_dir] [MEM_TARGET=...]"
	@echo ""
	@echo " Board Selection Options:"
	@echo "   BOARD=<name>      - Name of board subfolder under boards/ (default: mpfs-discovery-kit)"
	@echo "                       Examples:"
	@echo "                         make lim BOARD=mpfs-icicle-kit-es"
	@echo "                         make ddr BOARD=mpfs-video-kit"
	@echo "   BOARD_DIR=<path>  - Custom path to target board directory"
	@echo ""
	@echo " Primary Build Targets:"
	@echo "   lim               - Build application for LIM (Local Instruction Memory)"
	@echo "   envm              - Build application for eNVM (Non-Volatile Memory)"
	@echo "   envm-lim          - Build application for eNVM-LMA Scratchpad-VMA"
	@echo "   ddr               - Build application for volatile DDR Memory"
	@echo "   all               - Build executable images for active MEM_TARGET (default: lim)"
	@echo ""
	@echo " Code & Header Generation:"
	@echo "   generate_headers  - Parse Libero XML from active board to generate fpga_design_config"
	@echo ""
	@echo " Hardware Debugging:"
	@echo "   openocd           - Launch OpenOCD JTAG connection server"
	@echo "   debug             - Launch GDB session and connect to running OpenOCD server"
	@echo ""
	@echo " Maintenance & Diagnostics:"
	@echo "   show_size         - Display RAM/Flash memory usage for active target"
	@echo "   clean             - Delete all build directories and binary outputs"
	@echo "   help              - Display this help menu"
	@echo "========================================================================"

lim:
	@echo "=================================================="
	@echo " Building Target Application for LIM Workspace Memory"
	@echo " Board Path: $(BOARD_DIR)"
	@echo "=================================================="
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=lim all

envm:
	@echo "=================================================="
	@echo " Building Target Application for eNVM Non-Volatile Memory"
	@echo " Board Path: $(BOARD_DIR)"
	@echo "=================================================="
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=envm all

envm-lim:
	@echo "=================================================="
	@echo " Building Target Application for eNVM Non-Volatile Memory"
	@echo " Board Path: $(BOARD_DIR)"
	@echo "=================================================="
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=envm-lim all

ddr:
	@echo "=================================================="
	@echo " Building Target Application for Volatile DDR Controller Space"
	@echo " Board Path: $(BOARD_DIR)"
	@echo "=================================================="
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
	@echo " [LD]  Linking complex payload: $@"
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
	@$(SIZE) $<
	@echo "=================================================="

clean:
	@echo " Removing all execution build trees..."
	@rm -rf build

# ==============================================================================
# Debugging Automation Targets
# ==============================================================================

openocd:
	@echo "=================================================="
	@echo " Launching OpenOCD Server for PolarFire SoC..."
	@echo " Press Ctrl+C to terminate connection server."
	@echo "=================================================="
	$(OPENOCD) $(OPENOCD_CFG)

debug:
	@if [ ! -f $(BUILD_DIR)/$(TARGET).elf ]; then \
		echo "ERROR: Executable not found for [$(MEM_TARGET)] memory context. Run 'make $(MEM_TARGET)' first."; \
		exit 1; \
	fi
	@echo "=================================================="
	@echo " Starting GDB Session for: $(BUILD_DIR)/$(TARGET).elf"
	@echo " Target Architecture Sync context: $(MEM_TARGET)"
	@echo "=================================================="
	$(GDB) $(BUILD_DIR)/$(TARGET).elf \
		-ex "target remote localhost:$(GDB_PORT)" \
		-ex "monitor reset halt" \
		-ex "load"
