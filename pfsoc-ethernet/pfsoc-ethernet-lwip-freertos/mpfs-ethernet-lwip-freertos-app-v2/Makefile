# ==============================================================================
# PolarFire SoC FreeRTOS + lwIP Multi-Directory Makefile (Root Layout)
# Matched 1:1 with Working Configuration
# ==============================================================================

CROSS_COMPILE ?= riscv-none-elf-

CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size
GDB     := $(CROSS_COMPILE)gdb

TARGET  := pfsoc_app

# ------------------------------------------------------------------------------
# Tooling Paths & OS Auto-Detection
# ------------------------------------------------------------------------------
SOC_GEN_SCRIPT := platform/soc_config_generator/mpfs_configuration_generator.py

ifeq ($(OS),Windows_NT)
    PYTHON  ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/python3/python.exe
    OPENOCD ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/openocd/bin/openocd.exe
else
    PYTHON  ?= python3
    OPENOCD ?= /opt/microchip/SoftConsole/SoftConsole-v2022/openocd/bin/openocd
endif

OPENOCD_CFG  ?= -c "set DEVICE MPFS" -f board/microsemi-riscv.cfg
GDB_PORT     ?= 3333

# ------------------------------------------------------------------------------
# Board & Memory Target Configuration
# ------------------------------------------------------------------------------
BOARD     ?= mpfs-discovery-kit
BOARD_DIR := boards/$(BOARD)

MEM_TARGET ?= lim

ifeq ($(MEM_TARGET),envm)
    BUILD_DIR     := build/envm
    CONFIG_DIR    := envm-release
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/$(CONFIG_DIR)/linker/mpfs-envm.ld
else ifeq ($(MEM_TARGET),ddr)
    BUILD_DIR     := build/ddr
    CONFIG_DIR    := ddr-release
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/$(CONFIG_DIR)/linker/mpfs-ddr-loaded-by-boot-loader.ld
    CFLAGS        += -DIMAGE_LOADED_BY_BOOTLOADER=1
else
    BUILD_DIR     := build/lim
    CONFIG_DIR    := lim-debug
    LINKER_SCRIPT := $(BOARD_DIR)/platform_config/$(CONFIG_DIR)/linker/mpfs-lim.ld
    #CFLAGS        += -DIMAGE_LOADED_BY_BOOTLOADER=1
endif

export MEM_TARGET

# ------------------------------------------------------------------------------
# Architecture & Compiler Definitions
# ------------------------------------------------------------------------------
ARCH_FLAGS := -march=rv64gc \
              -mabi=lp64d \
              -mcmodel=medany \
              -msmall-data-limit=8 \
              -mstrict-align \
              -mno-save-restore

GLOBAL_DEFS := -DMPFS_DISCOVERY_KIT \
               -DTARGET_G5_SOC \
               -DTARGET_DISCOVERY_KIT \
               -DMSS_MAC_SIMPLE_TX_QUEUE \
               -DGEM0_MSS_MDIO \
               -DportasmHANDLE_INTERRUPT=handle_m_ext_interrupt

# ------------------------------------------------------------------------------
# Include Directories (Root Directory Layout)
# ------------------------------------------------------------------------------
INCLUDE_DIRS := . \
                application \
                platform \
                $(BOARD_DIR) \
                $(BOARD_DIR)/platform_config/$(CONFIG_DIR) \
                middleware \
                middleware/lwip \
                middleware/lwip/include \
                middleware/lwip/port \
                middleware/lwip/port/arch \
                middleware/FreeRTOS/include \
                middleware/FreeRTOS/portable/GCC/RISC-V \
                middleware/FreeRTOS/portable/GCC/RISC-V/chip_specific_extensions/RV32I_CLINT_no_extensions

INC_FLAGS := $(addprefix -I, $(INCLUDE_DIRS))

# ------------------------------------------------------------------------------
# Source Discovery
# ------------------------------------------------------------------------------
APP_SRCS_C := $(wildcard application/*.c) \
              $(wildcard application/inc/*.c) \
              $(wildcard application/hart0/*.c) \
              $(wildcard application/hart1/*.c) \
              $(wildcard application/hart2/*.c) \
              $(wildcard application/hart3/*.c) \
              $(wildcard application/hart4/*.c)

HAL_SRCS_C := $(wildcard platform/mpfs_hal/startup_gcc/*.c) \
              $(wildcard platform/mpfs_hal/common/*.c) \
              $(wildcard platform/mpfs_hal/common/nwc/*.c) \
              $(wildcard platform/hal/*.c) \
              $(wildcard platform/drivers/mss/*/*.c)

LWIP_SRCS_C := $(wildcard middleware/lwip/core/*.c) \
               $(wildcard middleware/lwip/core/ipv4/*.c) \
               $(wildcard middleware/lwip/core/ipv6/*.c) \
               $(wildcard middleware/lwip/api/*.c) \
               $(wildcard middleware/lwip/netif/*.c) \
               $(wildcard middleware/lwip/port/arch/*.c) \
               middleware/lwip/apps/http/httpd.c \
               middleware/lwip/apps/lwiperf/lwiperf.c

FREERTOS_SRCS_C := $(wildcard middleware/FreeRTOS/*.c) \
                   middleware/FreeRTOS/portable/MemMang/heap_4.c \
                   middleware/FreeRTOS/portable/GCC/RISC-V/port.c

SRCS_ASM := $(wildcard platform/mpfs_hal/startup_gcc/*.S) \
            $(wildcard platform/hal/*.S) \
            middleware/FreeRTOS/portable/GCC/RISC-V/portASM.S

SRCS_C   := $(sort $(APP_SRCS_C) $(HAL_SRCS_C) $(LWIP_SRCS_C) $(FREERTOS_SRCS_C))
SRCS_ASM := $(sort $(SRCS_ASM))

OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS_C)) \
        $(patsubst %.S, $(BUILD_DIR)/%.o, $(SRCS_ASM))

# ------------------------------------------------------------------------------
# Compiler & Linker Flags (Matched to SoftConsole Build Log)
# ------------------------------------------------------------------------------
CFLAGS := $(ARCH_FLAGS) \
          -O0 \
          -fmessage-length=0 \
          -fsigned-char \
          -ffunction-sections \
          -fdata-sections \
          -pedantic \
          -g3 \
          -std=gnu11 \
          -Wstrict-prototypes \
          -Wbad-function-cast \
          --specs=nano.specs \
          $(GLOBAL_DEFS) \
          $(INC_FLAGS)

ASFLAGS := $(ARCH_FLAGS) \
           -O0 \
           -fmessage-length=0 \
           -fsigned-char \
           -ffunction-sections \
           -fdata-sections \
           -pedantic \
           -g3 \
           -x assembler-with-cpp \
           --specs=nano.specs \
           $(GLOBAL_DEFS) \
           $(INC_FLAGS)

LDFLAGS := $(ARCH_FLAGS) \
           -O0 \
           -fmessage-length=0 \
           -fsigned-char \
           -ffunction-sections \
           -fdata-sections \
           -pedantic \
           -g3 \
           -T "$(LINKER_SCRIPT)" \
           -nostartfiles \
           -Xlinker --gc-sections \
           -Wl,-Map="$(BUILD_DIR)/$(TARGET).map" \
           --specs=nano.specs \
           --specs=nosys.specs

# ------------------------------------------------------------------------------
# Build Rules
# ------------------------------------------------------------------------------
.PHONY: all clean lim generate_headers openocd debug

lim:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR)MEM_TARGET=lim all
ddr:
	@$(MAKE) BOARD=$(BOARD) BOARD_DIR=$(BOARD_DIR) MEM_TARGET=ddr all    

generate_headers:
	@echo "Generating hardware configuration headers..."
	@$(PYTHON) $(SOC_GEN_SCRIPT) $(BOARD_DIR)/fpga_design/design_description/ $(BOARD_DIR)

all: generate_headers $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex show_size

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@echo " [LD]  Linking payload ($@) using script: $(LINKER_SCRIPT)"
	@$(CC) $(OBJS) $(LDFLAGS) -o $@    

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo " [CC] Compiling: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo " [AS] Assembling: $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	@echo " [OBJ] Extracting flash image: $@"
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
	@rm -rf build    



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

openocd:
	@echo "=================================================="
	@echo " Launching OpenOCD Server for PolarFire SoC..."
	@echo " Press Ctrl+C to terminate connection server."
	@echo "=================================================="
	$(OPENOCD) $(OPENOCD_CFG)

debug:
	@ELF_PATH="$(BUILD_DIR)/$(TARGET).elf"; \
	if [ ! -f "$$ELF_PATH" ]; then \
		echo "[ERROR] Executable $$ELF_PATH not found. Run 'make $(MEM_TARGET)' first."; \
		exit 1; \
	fi; \
	echo " Launching GDB Debug Session..."; \
	$(GDB) $$ELF_PATH \
		-ex 'set $$target_riscv=1' \
		-ex 'set mem inaccessible-by-default off' \
		-ex 'target extended-remote localhost:$(GDB_PORT)' \
		-ex 'monitor reset halt' \
		-ex 'load' \
		-ex 'thread apply all set $$pc = _start' \
		-ex 'break u54_1' \
		-ex 'continue'

debug-ddr:
	@ELF_PATH="build/ddr/$(TARGET).elf"; \
	if [ ! -f "$$ELF_PATH" ]; then \
		echo "ERROR: Executable not found at $$ELF_PATH. Run 'make ddr' first."; \
		exit 1; \
	fi; \
	echo "=================================================="; \
	echo " Attaching to Trained DDR Target: $$ELF_PATH"; \
	echo "=================================================="; \
	$(GDB) $$ELF_PATH \
		-ex 'set $$target_riscv=1' \
		-ex 'set mem inaccessible-by-default off' \
		-ex 'target extended-remote localhost:$(GDB_PORT)' \
		-ex 'interrupt' \
		-ex 'load' \
		-ex 'thread apply all set $$pc = _start' \
		-ex 'break u54_1' \
		-ex 'continue'
attach-ddr:
	$(GDB) build/ddr/$(TARGET).elf \
		-ex 'set $$target_riscv=1' \
		-ex 'set mem inaccessible-by-default off' \
		-ex 'target extended-remote localhost:$(GDB_PORT)' \
		-ex 'interrupt'	
gdbinit:
	@echo "Creating local .gdbinit..."
	@echo "set \$$target_riscv=1" > .gdbinit
	@echo "set mem inaccessible-by-default off" >> .gdbinit
	@echo "target extended-remote localhost:$(GDB_PORT)" >> .gdbinit
	@echo "file $(BUILD_DIR)/$(TARGET).elf" >> .gdbinit
	@echo ".gdbinit created for $(BUILD_DIR)/$(TARGET).elf."