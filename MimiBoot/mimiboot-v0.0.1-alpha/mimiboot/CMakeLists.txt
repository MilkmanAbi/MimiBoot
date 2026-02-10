# MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
# 
# CMakeLists.txt - Build configuration for RP2040/RP2350
#
# Usage:
#   mkdir build && cd build
#   cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
#   make
#
# Output: mimiboot.uf2

cmake_minimum_required(VERSION 3.13)

# ==============================================================================
# Pico SDK Configuration (before project())
# ==============================================================================

# Set Pico SDK path if not provided
if(NOT DEFINED PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    else()
        message(FATAL_ERROR "PICO_SDK_PATH not defined. Set via -DPICO_SDK_PATH or environment variable.")
    endif()
endif()

# Select board (pico or pico2)
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico CACHE STRING "Target board")
endif()

# RP2350 specific
if(PICO_BOARD STREQUAL "pico2")
    set(PICO_PLATFORM rp2350 CACHE STRING "Target platform")
    add_compile_definitions(TARGET_RP2350=1)
else()
    add_compile_definitions(TARGET_RP2040=1)
endif()

# Include Pico SDK
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# ==============================================================================
# Project Definition
# ==============================================================================

project(mimiboot C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize SDK
pico_sdk_init()

# ==============================================================================
# Compiler Flags
# ==============================================================================

# Optimize for size
add_compile_options(-Os)

# Strict warnings
add_compile_options(
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
    -Wno-unused-function
    -ffunction-sections
    -fdata-sections
)

# Definitions
add_compile_definitions(
    PICO_SDK=1
    MIMIBOOT_VERSION="0.0.1"
)

# ==============================================================================
# MimiBoot Executable
# ==============================================================================

add_executable(mimiboot
    # Main entry
    src/main.c
    
    # Core components
    src/core/loader.c
    src/core/config.c
    src/core/handoff.c
    
    # Filesystem
    src/fs/fat32.c
    
    # HAL - RP2040/RP2350
    src/hal/rp2040/hal_rp2040.c
    src/hal/rp2040/sd_spi.c
)

# Include directories
target_include_directories(mimiboot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link Pico SDK libraries (minimal set)
# We use pico_stdlib for the boot stage2 and minimal runtime
target_link_libraries(mimiboot
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_uart
    hardware_timer
    hardware_resets
)

# Linker options
target_link_options(mimiboot PRIVATE
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)

# Generate UF2 output
pico_add_extra_outputs(mimiboot)

# Disable USB and UART stdio (we do our own)
pico_enable_stdio_usb(mimiboot 0)
pico_enable_stdio_uart(mimiboot 0)

# ==============================================================================
# Example Payload (optional)
# ==============================================================================

option(BUILD_EXAMPLE_PAYLOAD "Build example payload" ON)

if(BUILD_EXAMPLE_PAYLOAD)
    add_executable(payload_blink
        examples/payload_blink/main.c
        examples/payload_blink/startup.c
    )
    
    target_include_directories(payload_blink PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # Use custom linker script for RAM execution
    set_target_properties(payload_blink PROPERTIES
        LINK_FLAGS "-T${CMAKE_CURRENT_SOURCE_DIR}/examples/payload_blink/payload.ld"
    )
    
    target_link_libraries(payload_blink
        pico_stdlib
        hardware_gpio
    )
    
    # Generate ELF (the format we load)
    pico_add_extra_outputs(payload_blink)
    
    # Copy ELF to known location
    add_custom_command(TARGET payload_blink POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:payload_blink>
            ${CMAKE_BINARY_DIR}/boot/kernel.elf
        COMMENT "Copying payload ELF to boot/kernel.elf"
    )
endif()

# ==============================================================================
# Size Report
# ==============================================================================

add_custom_command(TARGET mimiboot POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:mimiboot>
    COMMENT "MimiBoot size:"
)

# ==============================================================================
# Install Targets
# ==============================================================================

install(TARGETS mimiboot
    RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_BINARY_DIR}/mimiboot.uf2
    DESTINATION bin
    OPTIONAL
)

# ==============================================================================
# Documentation
# ==============================================================================

message(STATUS "")
message(STATUS "MimiBoot Configuration:")
message(STATUS "  Board: ${PICO_BOARD}")
message(STATUS "  SDK Path: ${PICO_SDK_PATH}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Build with: make")
message(STATUS "Flash with: copy mimiboot.uf2 to Pico in BOOTSEL mode")
message(STATUS "")
