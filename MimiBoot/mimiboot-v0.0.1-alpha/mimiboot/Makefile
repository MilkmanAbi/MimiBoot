# MimiBoot - Minimal Second-Stage Bootloader
#
# Convenience Makefile wrapper for CMake
#
# Usage:
#   make              - Build everything
#   make clean        - Clean build directory
#   make flash        - Build and copy UF2 (macOS)
#   make payload      - Build only the example payload
#   make debug        - Build with debug symbols
#   make pico2        - Build for RP2350/Pico 2

# Configuration
BUILD_DIR    := build
PICO_SDK_PATH ?= $(HOME)/pico-sdk
CMAKE_FLAGS  := -DPICO_SDK_PATH=$(PICO_SDK_PATH)

# Default target
.PHONY: all
all: build

# Create build directory and run CMake
$(BUILD_DIR)/Makefile:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) ..

# Build everything
.PHONY: build
build: $(BUILD_DIR)/Makefile
	@$(MAKE) -C $(BUILD_DIR)

# Build for Pico 2 (RP2350)
.PHONY: pico2
pico2:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) -DPICO_BOARD=pico2 ..
	@$(MAKE) -C $(BUILD_DIR)

# Debug build
.PHONY: debug
debug:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=Debug ..
	@$(MAKE) -C $(BUILD_DIR)

# Build only payload
.PHONY: payload
payload: $(BUILD_DIR)/Makefile
	@$(MAKE) -C $(BUILD_DIR) payload_blink

# Clean
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)

# Deep clean (including SDK cache)
.PHONY: distclean
distclean: clean
	@rm -rf CMakeCache.txt CMakeFiles

# Flash to Pico (macOS)
# Assumes Pico is in BOOTSEL mode and mounted at /Volumes/RPI-RP2
.PHONY: flash
flash: build
	@if [ -d "/Volumes/RPI-RP2" ]; then \
		cp $(BUILD_DIR)/mimiboot.uf2 /Volumes/RPI-RP2/; \
		echo "Flashed mimiboot.uf2 to Pico"; \
	else \
		echo "Error: Pico not found. Hold BOOTSEL and plug in."; \
		exit 1; \
	fi

# Create SD card structure
.PHONY: sdcard
sdcard: payload
	@mkdir -p sdcard/boot
	@cp $(BUILD_DIR)/payload_blink.elf sdcard/boot/kernel.elf
	@cp examples/boot.cfg sdcard/
	@echo "SD card contents ready in ./sdcard/"
	@echo "Copy contents to FAT32-formatted SD card"

# Show size report
.PHONY: size
size: build
	@arm-none-eabi-size $(BUILD_DIR)/mimiboot.elf
	@echo ""
	@arm-none-eabi-size $(BUILD_DIR)/payload_blink.elf

# Generate disassembly
.PHONY: disasm
disasm: build
	@arm-none-eabi-objdump -d $(BUILD_DIR)/mimiboot.elf > $(BUILD_DIR)/mimiboot.lst
	@echo "Disassembly written to $(BUILD_DIR)/mimiboot.lst"

# Help
.PHONY: help
help:
	@echo "MimiBoot Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build bootloader and example payload (default)"
	@echo "  build     - Same as 'all'"
	@echo "  pico2     - Build for RP2350/Pico 2"
	@echo "  debug     - Build with debug symbols"
	@echo "  payload   - Build only the example payload"
	@echo "  clean     - Remove build directory"
	@echo "  flash     - Flash to Pico (macOS, BOOTSEL mode)"
	@echo "  sdcard    - Prepare SD card contents"
	@echo "  size      - Show binary sizes"
	@echo "  disasm    - Generate disassembly listing"
	@echo ""
	@echo "Environment:"
	@echo "  PICO_SDK_PATH - Path to Pico SDK (default: ~/pico-sdk)"
	@echo ""
	@echo "Example:"
	@echo "  make PICO_SDK_PATH=/opt/pico-sdk pico2"
