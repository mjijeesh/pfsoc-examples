# ==============================================================================
# Advanced Multi-Directory PolarFire SoC Makefile
# Configured for LiteX BIOS / Bare-Metal Port (SoftConsole Identical Flags)
# Supports Memory Targets: eNVM, LIM, DDR
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

OPENOCD ?= openocd
TARGET  := pfsoc_app

# ------------------------------------------------------------------------------
# Dynamic Board Configuration & Validation
# ------------------------------------------------------------------------------
BOARD     ?= mpfs-discovery-kit
BOARD_DIR ?= boards/$(BOARD)

AVAILABLE_BOARDS := $(notdir $(wildcard boards/*))

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
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/lim-debug/linker/mpfs-lim.ld
    CONFIG_DIR    := lim-debug
endif

# ------------------------------------------------------------------------------
# Core Architecture Flags (Matched to Microchip SoftConsole Settings)
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

INCLUDE_DIRS += $(BOARD_DIR)/platform_config/$(CONFIG_DIR) \
                $(BOARD_DIR)/platform_config/$(CONFIG_DIR)/mpfs_hal_config \
                application \
                platform \
                $(BOARD_DIR) \
                $(DRIVER_DIRS)
				

INC_FLAGS := $(addprefix -I, $(INCLUDE_DIRS))

# Discover C and Assembly sources (sort removes any accidental duplicates)
RAW_SRCS_C := $(wildcard application/*.c) \
              $(wildcard application/*/*.c) \
              $(wildcard platform/drivers/*/*.c) \
              $(wildcard platform/mpfs_hal/startup_gcc/*.c) \
              $(wildcard platform/mpfs_hal/common/*.c) \
              $(wildcard platform/mpfs_hal/common/nwc/*.c) \
              $(wildcard platform/hal/*.c) \
              platform/drivers/mss/mss_mmuart/mss_uart.c \
			  platform/drivers/mss/mss_gpio/mss_gpio.c

SRCS_C := $(sort $(RAW_SRCS_C))

SRCS_ASM := $(wildcard platform/hal/*.S) \
            $(wildcard platform/mpfs_hal/startup_gcc/*.S)

OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS_C)) \
        $(patsubst %.S, $(BUILD_DIR)/%.o, $(SRCS_ASM))

# ------------------------------------------------------------------------------
# Compilation & Linker Flags (Matched to Microchip SoftConsole Settings)
# ------------------------------------------------------------------------------
CFLAGS := $(ARCH_FLAGS) \
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
          
CFLAGS   += $(INC_FLAGS)

# Route standard newlib printf() to E51 MMUART0
CFLAGS += -DMICROCHIP_STDIO_THRU_MMUARTX=\&g_mss_uart0_lo


# optional flasgs if needed 
#CFLAGS  += -DIMAGE_LOADED_BY_BOOTLOADER=1
#-DMPFS_DISCOVERY_KIT \ 
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
# Core Automation Targets
# ==============================================================================

.PHONY: all clean show_size lim envm envm-lim ddr generate_headers openocd debug gdbinit help

help:
	@echo "========================================================================"
	@echo " PolarFire SoC Bare-Metal Build System"
	@echo " Active Board Path : $(BOARD_DIR)"
	@echo " Available Boards  : $(AVAILABLE_BOARDS)"
	@echo "========================================================================"
	@echo " Usage: make <target> [BOARD=<board_folder_name>] [MEM_TARGET=lim|envm|envm-lim|ddr]"
	@echo ""
	@echo " Primary Build Targets:"
	@echo "   lim               - Build application for LIM (Local Instruction Memory)"
	@echo "   envm              - Build application for eNVM (Non-Volatile Memory)"
	@echo "   ddr               - Build application for volatile DDR Memory"
	@echo "   all               - Build executable images for active MEM_TARGET (default: lim)"
	@echo ""
	@echo " Hardware Debugging:"
	@echo "   openocd           - Launch OpenOCD JTAG connection server"
	@echo "   debug             - Launch GDB session and connect to running OpenOCD server"
	@echo "========================================================================"

lim:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=lim all

envm:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=envm all

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
	@rm -rf build .gdbinit

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
		-ex 'set $$target_riscv=1' \
		-ex 'set mem inaccessible-by-default off' \
		-ex 'target extended-remote localhost:$(GDB_PORT)' \
		-ex 'monitor reset halt' \
		-ex 'load' \
		-ex 'thread apply all set $$pc = _start' \
		-ex 'break e51' \
		-ex 'continue'
gdbinit:
	@echo "Creating local .gdbinit..."
	@echo "set \$$target_riscv=1" > .gdbinit
	@echo "set mem inaccessible-by-default off" >> .gdbinit
	@echo "target extended-remote localhost:$(GDB_PORT)" >> .gdbinit
	@echo "file $(BUILD_DIR)/$(TARGET).elf" >> .gdbinit
	@echo ".gdbinit created for $(BUILD_DIR)/$(TARGET).elf."