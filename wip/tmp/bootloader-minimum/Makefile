# ==============================================================================
# Ultra-Lean PolarFire SoC Modular Makefile
# Isolates objects to the build directory
# ==============================================================================

CROSS_COMPILE ?= riscv-none-elf-

CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE    := $(CROSS_COMPILE)size

TARGET    := tiny_bootloader
BUILD_DIR := build

ARCH_FLAGS := -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany

CFLAGS := $(ARCH_FLAGS) -Wall -Wextra -O2 -ffreestanding -fno-builtin -I.
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Wl,--gc-sections -T bootloader.ld -Wl,--no-warn-rwx-segments

# Explicitly maintain all objects inside the build folder paths
OBJS := $(BUILD_DIR)/startup.o \
        $(BUILD_DIR)/mss_uart.o \
        $(BUILD_DIR)/main.o

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex
	@echo ""
	@$(SIZE) $(BUILD_DIR)/$(TARGET).elf

# Main linker target rule
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@echo " [LD]  Linking output execution payload: $@"
	@$(CC) $(OBJS) $(LDFLAGS) -o $@

# Pattern compilation for any C modules
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo " [CC]  Compiling file unit: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Pattern compilation for low-level assembly vectors
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo " [AS]  Assembling file unit: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	@$(OBJCOPY) -O ihex $< $@

clean:
	@echo " Removing build directory contents..."
	rm -rf $(BUILD_DIR)