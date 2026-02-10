# ELF Format Requirements for MimiBoot Payloads

This document describes the ELF file format requirements for payloads
loaded by MimiBoot.

## Overview

MimiBoot loads standard ELF32 executables into RAM. The loader is
intentionally simple - it reads program headers and copies PT_LOAD
segments to their specified virtual addresses.

## Requirements

### ELF Header

| Field | Required Value | Notes |
|-------|---------------|-------|
| e_ident[EI_MAG0..3] | 0x7F 'E' 'L' 'F' | Standard magic |
| e_ident[EI_CLASS] | ELFCLASS32 (1) | 32-bit ELF |
| e_ident[EI_DATA] | ELFDATA2LSB (1) | Little-endian |
| e_type | ET_EXEC (2) | Executable file |
| e_machine | EM_ARM (40) | ARM architecture |
| e_entry | Non-zero | Entry point address |
| e_phnum | > 0 | At least one program header |

### Program Headers

MimiBoot processes only `PT_LOAD` segments. Each loadable segment must:

1. **Fit in RAM**: `p_vaddr + p_memsz <= RAM_END`
2. **Be in RAM region**: `p_vaddr >= RAM_START`
3. **Not overlap**: Segments must not overlap each other

For each PT_LOAD segment, MimiBoot:
1. Copies `p_filesz` bytes from file offset `p_offset` to address `p_vaddr`
2. Zeros `p_memsz - p_filesz` bytes after the copied data (BSS)

### Memory Model

```
RAM Layout:
┌─────────────────────────────────────┐ RAM_END
│ Stack (grows down)                  │
├─────────────────────────────────────┤
│ Heap (optional, grows up)           │
├─────────────────────────────────────┤
│ .bss (uninitialized data)           │
├─────────────────────────────────────┤
│ .data (initialized data)            │
├─────────────────────────────────────┤
│ .rodata (read-only data)            │
├─────────────────────────────────────┤
│ .text (code)                        │
├─────────────────────────────────────┤
│ .vectors (vector table)             │
└─────────────────────────────────────┘ RAM_START (e.g., 0x20000000)
```

## Linker Script Requirements

Your payload's linker script must:

1. **Target RAM only** - all sections go to RAM
2. **Define entry point** - use `ENTRY(_entry)` or similar
3. **Align vector table** - Cortex-M requires 256-byte alignment
4. **Provide stack** - MimiBoot may reset SP, but payload should set it

Example linker script snippet:

```ld
MEMORY
{
    RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

ENTRY(_entry)

SECTIONS
{
    .vectors : ALIGN(256) {
        KEEP(*(.vectors))
    } > RAM

    .text : {
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        *(.data*)
    } > RAM

    .bss (NOLOAD) : {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    } > RAM

    .stack (NOLOAD) : {
        . = ORIGIN(RAM) + LENGTH(RAM) - 4096;
        __stack_top__ = ORIGIN(RAM) + LENGTH(RAM);
    } > RAM
}
```

## Startup Code Requirements

Your payload's startup code should:

1. **Set stack pointer** (MimiBoot may pass it, but don't rely on it)
2. **Zero BSS** (even though MimiBoot zeros it, be defensive)
3. **Accept handoff pointer** in r0 (optional)

Example startup:

```c
__attribute__((naked, section(".vectors")))
void _entry(void) {
    __asm__ volatile (
        "mov r4, r0              \n"  // Save handoff pointer
        "ldr r0, =__stack_top__  \n"
        "mov sp, r0              \n"
        "bl zero_bss             \n"
        "mov r0, r4              \n"  // Restore handoff pointer
        "bl main                 \n"
        "1: wfi                  \n"
        "b 1b                    \n"
    );
}
```

## Handoff Structure

MimiBoot passes a pointer to `mimi_handoff_t` in r0. Your payload can:

1. **Ignore it** - just run as if booted directly
2. **Use it** - get boot context, timing, memory layout

See `include/mimiboot/handoff.h` for structure definition.

## Building Payloads

### With Pico SDK

```cmake
add_executable(my_payload
    main.c
    startup.c
)

target_link_options(my_payload PRIVATE
    -T${CMAKE_SOURCE_DIR}/payload.ld
)
```

### With arm-none-eabi-gcc

```bash
arm-none-eabi-gcc \
    -mcpu=cortex-m0plus \
    -mthumb \
    -Os \
    -nostartfiles \
    -T payload.ld \
    -o payload.elf \
    main.c startup.c
```

## Verification

Use `readelf` to verify your payload:

```bash
# Check ELF header
arm-none-eabi-readelf -h payload.elf

# Check program headers (loadable segments)
arm-none-eabi-readelf -l payload.elf

# Check entry point
arm-none-eabi-readelf -h payload.elf | grep Entry
```

Example good output:

```
ELF Header:
  Magic:   7f 45 4c 46 01 01 01 00 ...
  Class:                             ELF32
  Data:                              2's complement, little endian
  Type:                              EXEC (Executable file)
  Machine:                           ARM
  Entry point address:               0x20000101

Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x010000 0x20000000 0x20000000 0x02000 0x04000 RWE 0x10000
```

## Common Issues

### "Not an ELF file"
- Check file isn't corrupted
- Ensure it's the .elf file, not .bin or .uf2

### "Not 32-bit ELF"
- Ensure building with `-m32` or for ARM32 target

### "Segment address outside RAM"
- Check linker script uses correct RAM addresses
- Verify segments don't exceed RAM size

### "Image too large"
- Reduce code/data size
- Check for accidentally included debug sections
- Strip with `arm-none-eabi-strip`

## Size Optimization

For minimal payloads:

```bash
# Strip all symbols
arm-none-eabi-strip payload.elf

# Or be more selective
arm-none-eabi-objcopy \
    --strip-debug \
    --strip-unneeded \
    payload.elf payload-stripped.elf
```

Linker flags for size:

```
-Wl,--gc-sections  # Remove unused sections
-Os                # Optimize for size
-ffunction-sections
-fdata-sections
```
