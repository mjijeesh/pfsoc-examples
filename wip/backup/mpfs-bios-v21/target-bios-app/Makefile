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

TARGET  := pfsoc_app

# Capture current build timestamp from host system
BUILD_TIMESTAMP := $(shell date +"%Y-%m-%d %H:%M:%S")
CFLAGS += -DBUILD_TIMESTAMP="\"$(BUILD_TIMESTAMP)\""


# ==============================================================================
# OS Detection & OpenOCD Executable Path Configuration
# ==============================================================================
ifeq ($(OS),Windows_NT)
    OPENOCD ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/openocd/bin/openocd.exe
else
    OPENOCD ?= /opt/microchip/SoftConsole/SoftConsole-v2022/openocd/bin/openocd
endif

# ==============================================================================
# FlashPro Express FPGA Job Programming Configuration
# ==============================================================================
ifeq ($(OS),Windows_NT)
    FPEXPRESS ?= C:/Microchip/Libero_SoC_v2025.2/Designer/bin64/FPExpress.exe
else
    FPEXPRESS ?= /opt/microchip/Libero_SoC_2025.2/Libero_SoC/Designer/bin64/FPExpress
endif


# ------------------------------------------------------------------------------
# Debugger Configuration
# ------------------------------------------------------------------------------
OPENOCD_CFG  ?= -c "set DEVICE MPFS" -f board/microsemi-riscv.cfg
GDB_PORT     ?= 3333


# ------------------------------------------------------------------------------
# Dynamic Board Configuration & Validation
# ------------------------------------------------------------------------------
BOARD     ?= mpfs-discovery-kit
BOARD_DIR := boards/$(BOARD)

AVAILABLE_BOARDS := $(notdir $(wildcard boards/*))

# Default Job File path based on selected board
JOB_FILE ?= $(BOARD_DIR)/fpga_design/$(BOARD).job

# Device defaults
ifeq ($(BOARD),mpfs-icicle-kit)
    DIE_DEFAULT     := MPFS250T
    PACKAGE_DEFAULT := FCVG484
else ifeq ($(BOARD),mpfs-video-kit)
    DIE_DEFAULT     := MPFS250T
    PACKAGE_DEFAULT := FCG1152
else
    DIE_DEFAULT     := MPFS095T
    PACKAGE_DEFAULT := FCSG325
endif


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
CRCFBIGEN      := $(PYTHON) tools/crcfbigen.py
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
# Dynamic Memory Target Configuration Switcher
# ------------------------------------------------------------------------------
MEM_TARGET ?= lim

ifeq ($(MEM_TARGET),envm)
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
    BUILD_DIR     := build/lim
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/lim-debug/linker/mpfs-lim.ld
    CONFIG_DIR    := lim-debug
endif

export MEM_TARGET

# ==============================================================================
# Pass Board Name and Memory Target directly to C Compiler (ADD LINES HERE)
# ==============================================================================
#CFLAGS += -DBOARD_NAME="\"$(BOARD)\""
#CFLAGS += -DMEM_TARGET="\"$(MEM_TARGET)\""


# Include board-specific Make fragment (loads board_config.mk defaults if present)
-include $(BOARD_DIR)/board.mk

# Fallback board defaults if board_config.mk is missing
CLI_PROMPT          ?= $(BOARD)>
DIE                 ?= MPFS095T
PACKAGE             ?= FCSG325
MSS_UART            ?= MSS_UART1
BAUD_RATE           ?= 115200
SPI_FLASH_ENGINE    ?= MSS_SPI0
FLASH_FAMILY        ?= AUTO
SPI_FLASH_BOOT_ADDR ?= 0x00100000
PAYLOAD_RAM_ADDR    ?= 0x80000000
ENABLE_SPI_FLASH    ?= 1
ENABLE_SDCARD       ?= 1
ENABLE_FATFS        ?= 1
JOB_FILE            ?= $(BOARD_DIR)/fpga_design/$(BOARD).job

# ------------------------------------------------------------------------------
# 4. MSS UART Port Symbol & Peripheral Clock Resolution
#
# Usage:
#   make MSS_UART=0           -> Selects UART 0 (g_mss_uart0_lo)
#   make MSS_UART=MSS_UART2   -> Selects UART 2 (g_mss_uart2_lo)
#   make                      -> Defaults to UART 0 if unset/invalid
# ------------------------------------------------------------------------------

# Extract number (strips "MSS_UART" prefix if present, e.g., MSS_UART0 -> 0)
_UART_NUM := $(patsubst MSS_UART%,%,$(MSS_UART))

# Fallback to 0 if _UART_NUM is not in the range [0, 4]
ifeq ($(filter $(_UART_NUM),0 1 2 3 4),)
    _UART_NUM := 0
endif

STDIO_UART_SYMBOL := g_mss_uart$(_UART_NUM)_lo
STDIO_UART_PERIPH := MSS_PERIPH_MMUART$(_UART_NUM)



# Forward Make overrides to C Preprocessor (-D flags)
CFLAGS += -DBOARD_NAME="\"$(BOARD)\""
CFLAGS += -DMPFS_TARGET_DIE="\"$(DIE)\""
CFLAGS += -DMPFS_TARGET_PACKAGE="\"$(PACKAGE)\""
CFLAGS += -DMICROCHIP_STDIO_BAUD_RATE=$(BAUD_RATE)UL
CFLAGS += -DMICROCHIP_STDIO_THRU_MMUARTX=\&$(STDIO_UART_SYMBOL)
CFLAGS += -DMICROCHIP_STDIO_PERIPH=$(STDIO_UART_PERIPH)

# Feature Flags (-D defines)
CFLAGS += -DENABLE_SPI_FLASH=$(ENABLE_SPI_FLASH)
CFLAGS += -DENABLE_SDCARD=$(ENABLE_SDCARD)
CFLAGS += -DENABLE_FATFS=$(ENABLE_FATFS)

# Address Configuration Flags
CFLAGS += -DSPI_FLASH_BOOT_ADDR=$(SPI_FLASH_BOOT_ADDR)UL
CFLAGS += -DPAYLOAD_RAM_ADDR=$(PAYLOAD_RAM_ADDR)UL
CFLAGS += -DCLI_PROMPT="\"$(CLI_PROMPT)\""



# ------------------------------------------------------------------------------
# Core Architecture Flags
# ------------------------------------------------------------------------------
ARCH_FLAGS := -march=rv64gc \
              -mabi=lp64d \
              -mcmodel=medany \
              -msmall-data-limit=8 \
              -mstrict-align \
              -mno-save-restore

# ==============================================================================
# Include Directories (Categorized)
# ==============================================================================

# Application Includes
APP_INC_DIRS := application \
                application/bios \
                application/bios/commands \
                application/drivers/flash \
                application/drivers/uart \
                application/utils

# Driver Includes
DRIVER_INC_DIRS := platform/drivers/mss/mss_mmuart \
                   platform/drivers/mss/mss_gpio \
                   platform/drivers/mss/mss_spi \
                   platform/drivers/mss/mss_sys_services \
                   platform/drivers/fpga_ip/CoreSPI

# HAL & Hardware Configuration Includes
HAL_INC_DIRS := $(BOARD_DIR)/platform_config/$(CONFIG_DIR) \
                $(BOARD_DIR)/platform_config/$(CONFIG_DIR)/mpfs_hal_config \
                platform \
                $(BOARD_DIR)

# Combined Includes
INCLUDE_DIRS := $(APP_INC_DIRS) $(DRIVER_INC_DIRS) $(HAL_INC_DIRS)
INC_FLAGS    := $(addprefix -I, $(INCLUDE_DIRS))

# ==============================================================================
# Source Files Discovery (Categorized)
# ==============================================================================

# Application Sources
APP_SRCS_C := $(wildcard application/*.c) \
              $(wildcard application/*/*.c) \
              $(wildcard application/*/*/*.c)

# Peripheral Driver Sources (Explicitly specified to avoid unused drivers)
DRIVER_SRCS_C := platform/drivers/mss/mss_mmuart/mss_uart.c \
                 platform/drivers/mss/mss_gpio/mss_gpio.c \
                 platform/drivers/mss/mss_spi/mss_spi.c \
                 platform/drivers/fpga_ip/CoreSPI/core_spi.c \
                 platform/drivers/mss/mss_sys_services/mss_sys_services.c

# Hardware Abstraction Layer (HAL) Sources
HAL_SRCS_C := $(wildcard platform/mpfs_hal/startup_gcc/*.c) \
              $(wildcard platform/mpfs_hal/common/*.c) \
              $(wildcard platform/mpfs_hal/common/nwc/*.c) \
              $(wildcard platform/hal/*.c)

HAL_SRCS_ASM := $(wildcard platform/hal/*.S) \
                $(wildcard platform/mpfs_hal/startup_gcc/*.S)

# Sorted/Deduplicated Source Collections
SRCS_C   := $(sort $(APP_SRCS_C) $(DRIVER_SRCS_C) $(HAL_SRCS_C))
SRCS_ASM := $(sort $(HAL_SRCS_ASM))

# Target Object Files
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

# ==============================================================================
# Core Automation Targets
# ==============================================================================
.PHONY: all clean show_size lim lim-app envm envm-lim ddr generate_headers openocd debug gdbinit help program-envm program-envm-lim flash-envm program-job print_build_info

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
	@echo "   program-job       - Flash FPGA fabric using a .job file (JOB_FILE=<path>)"
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
print_build_info:
	@echo "========================================================================"
	@echo "                   PolarFire SoC BIOS Build Context                     "
	@echo "========================================================================"
	@echo "  * BOARD_NAME                      : $(BOARD) ($(BOARD_DIR))"
	@echo "  * MPFS_TARGET_DIE / PACKAGE       : $(DIE) / $(PACKAGE)"
	@echo "  * Target Memory                   : $(MEM_TARGET) (Linker: $(LINKER_SCRIPT))"
	@echo "  * MICROCHIP_STDIO_THRU_MMUARTX    : $(MSS_UART) (&$(STDIO_UART_SYMBOL), Periph: $(STDIO_UART_PERIPH))"
	@echo "  * MICROCHIP_STDIO_BAUD_RATE       : $(BAUD_RATE) bps"
	@echo "  * ENABLE_SPI_FLASH                : $(ENABLE_SPI_FLASH) (Engine: $(SPI_FLASH_ENGINE), Family: $(FLASH_FAMILY))"
	@echo "  * SPI_FLASH_BOOT_ADDR             : $(SPI_FLASH_BOOT_ADDR)"
	@echo "  * PAYLOAD_RAM_ADDR                : $(PAYLOAD_RAM_ADDR)"
	@echo "  * CLI_PROMPT                      : $(CLI_PROMPT)"
	@echo "  * ENABLE_SDCARD                   : $(ENABLE_SDCARD)"
	@echo "  * ENABLE_FATFS                    : $(ENABLE_FATFS)"
	@echo "========================================================================"


all: print_build_info generate_headers $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).fbi show_size

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

$(BUILD_DIR)/$(TARGET).fbi: $(BUILD_DIR)/$(TARGET).bin
	@echo " [FBI] Generating Flash Binary Image with CRC32: $@"
	@$(CRCFBIGEN) $< -o $@ --fbi --little

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

program-job:
	@echo "=================================================="
	@echo " Programming FPGA Fabric with Job File"
	@echo " Job File     : $(JOB_FILE)"
	@echo " Target Board : $(BOARD)"
	@echo "=================================================="
	@if [ ! -f "$(JOB_FILE)" ]; then \
		echo "[ERROR] Job file '$(JOB_FILE)' was not found!"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_DIR)/fp_job
	@echo "create_job_project -job_project_location {$(CURDIR)/$(BUILD_DIR)/fp_job} -job_file {$(CURDIR)/$(JOB_FILE)} -overwrite 1" > $(BUILD_DIR)/program_job.tcl
	@echo "run_selected_actions" >> $(BUILD_DIR)/program_job.tcl
	@echo "close_project" >> $(BUILD_DIR)/program_job.tcl
	$(FPEXPRESS) SCRIPT:$(BUILD_DIR)/program_job.tcl

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
