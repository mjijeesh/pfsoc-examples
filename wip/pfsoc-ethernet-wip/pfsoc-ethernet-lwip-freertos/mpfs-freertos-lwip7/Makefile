# ==============================================================================
# PolarFire SoC FreeRTOS + lwIP Multi-Directory Makefile
# Matched 1:1 with Microchip SoftConsole v2022.2 C/C++ & Cross-Assembler Flags
# ==============================================================================

#CROSS_COMPILE ?= riscv64-unknown-elf-
CROSS_COMPILE ?= riscv-none-elf-

CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size
GDB     := $(CROSS_COMPILE)gdb

TARGET  := mpfs-freertos-lwip


# Tooling Paths
SOC_GEN_SCRIPT := src/platform/soc_config_generator/mpfs_configuration_generator.py

ifeq ($(OS),Windows_NT)
    PYTHON  ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/python3/python.exe
    OPENOCD ?= C:/Microchip/SoftConsole-v2022.2-RISC-V-747/openocd/bin/openocd.exe
else
    PYTHON  ?= python3
    OPENOCD ?= /opt/microchip/SoftConsole/SoftConsole-v2022/openocd/bin/openocd
endif



OPENOCD_CFG  ?= -c "set DEVICE MPFS" -f board/microsemi-riscv.cfg
GDB_PORT     ?= 3333

# Board & Memory Target
BOARD     ?= mpfs-discovery-kit
BOARD_DIR := src/boards/$(BOARD)

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
endif

# Architecture Flags
ARCH_FLAGS := -march=rv64gc \
              -mabi=lp64d \
              -mcmodel=medany \
              -msmall-data-limit=8 \
              -mstrict-align \
              -mno-save-restore

# Global Compiler Definitions (Extracted from SoftConsole Build Log)
GLOBAL_DEFS := -DMPFS_DISCOVERY_KIT \
               -DTARGET_G5_SOC \
               -DTARGET_DISCOVERY_KIT \
               -DMSS_MAC_SIMPLE_TX_QUEUE \
               -DGEM0_MSS_MDIO \
               -DportasmHANDLE_INTERRUPT=handle_m_ext_interrupt

# Include Directories (Matched with SoftConsole Log)
INCLUDE_DIRS := src \
                src/application \
                src/platform \
                $(BOARD_DIR) \
                $(BOARD_DIR)/platform_config/$(CONFIG_DIR) \
                src/middleware \
                src/middleware/lwip \
                src/middleware/lwip/include \
                src/middleware/lwip/port \
                src/middleware/lwip/port/arch \
                src/middleware/FreeRTOS/include \
                src/middleware/FreeRTOS/portable/GCC/RISC-V \
                src/middleware/FreeRTOS/portable/GCC/RISC-V/chip_specific_extensions/RV32I_CLINT_no_extensions

INC_FLAGS := $(addprefix -I, $(INCLUDE_DIRS))

# Source Files Discovery
APP_SRCS_C := $(wildcard src/application/*.c) \
              $(wildcard src/application/inc/*.c) \
              $(wildcard src/application/hart0/*.c) \
              $(wildcard src/application/hart1/*.c) \
              $(wildcard src/application/hart2/*.c) \
              $(wildcard src/application/hart3/*.c) \
              $(wildcard src/application/hart4/*.c)

HAL_SRCS_C := $(wildcard src/platform/mpfs_hal/startup_gcc/*.c) \
              $(wildcard src/platform/mpfs_hal/common/*.c) \
              $(wildcard src/platform/mpfs_hal/common/nwc/*.c) \
              $(wildcard src/platform/hal/*.c) \
              $(wildcard src/platform/drivers/mss/*/*.c)

LWIP_SRCS_C := $(wildcard src/middleware/lwip/core/*.c) \
               $(wildcard src/middleware/lwip/core/ipv4/*.c) \
               $(wildcard src/middleware/lwip/core/ipv6/*.c) \
               $(wildcard src/middleware/lwip/api/*.c) \
               $(wildcard src/middleware/lwip/netif/*.c) \
               $(wildcard src/middleware/lwip/port/arch/*.c) \
               src/middleware/lwip/apps/http/httpd.c \
               src/middleware/lwip/apps/lwiperf/lwiperf.c

FREERTOS_SRCS_C := $(wildcard src/middleware/FreeRTOS/*.c) \
                   src/middleware/FreeRTOS/portable/MemMang/heap_4.c \
                   src/middleware/FreeRTOS/portable/GCC/RISC-V/port.c

SRCS_ASM := $(wildcard src/platform/mpfs_hal/startup_gcc/*.S) \
            $(wildcard src/platform/hal/*.S) \
            src/middleware/FreeRTOS/portable/GCC/RISC-V/portASM.S

SRCS_C   := $(sort $(APP_SRCS_C) $(HAL_SRCS_C) $(LWIP_SRCS_C) $(FREERTOS_SRCS_C))
SRCS_ASM := $(sort $(SRCS_ASM))

OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS_C)) \
        $(patsubst %.S, $(BUILD_DIR)/%.o, $(SRCS_ASM))

# SoftConsole C & Assembler Compiler Flags
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

.PHONY: all clean lim generate_headers openocd debug

lim:
	@$(MAKE) MEM_TARGET=lim all

generate_headers:
	@echo "Generating hardware configuration headers..."
	@$(PYTHON) $(SOC_GEN_SCRIPT) $(BOARD_DIR)/fpga_design/design_description/ $(BOARD_DIR)

all: generate_headers $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex show_size

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@echo " [LD] Linking executable target: $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

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

show_size: $(BUILD_DIR)/$(TARGET).elf
	@echo ""
	@$(SIZE) --format=sysv --totals --radix=16 $<

clean:
	@rm -rf build

openocd:
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