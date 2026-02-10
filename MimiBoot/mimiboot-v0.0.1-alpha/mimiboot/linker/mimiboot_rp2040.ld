/**
 * MimiBoot Linker Script for RP2040
 * 
 * Memory Layout:
 * 
 * Flash (XIP):
 *   0x10000000 - 0x100000FF : boot2 (256 bytes, Pico SDK stage2)
 *   0x10000100 - 0x10003FFF : MimiBoot (~16KB)
 *   0x10004000+ : Available for other uses
 * 
 * RAM:
 *   0x20000000 - 0x20041FFF : SRAM (264KB on RP2040)
 *   
 * MimiBoot uses minimal RAM during operation:
 *   - Stack: 2KB
 *   - Buffers: 1KB
 *   - Static data: ~1KB
 * 
 * After loading payload, all RAM is available for the payload.
 */

MEMORY
{
    /* boot2 lives in first 256 bytes */
    BOOT2(rx)  : ORIGIN = 0x10000000, LENGTH = 256
    
    /* MimiBoot code in flash after boot2 */
    FLASH(rx)  : ORIGIN = 0x10000100, LENGTH = 16128   /* 16KB - 256 */
    
    /* RAM for stack and data during boot */
    RAM(rwx)   : ORIGIN = 0x20000000, LENGTH = 264K
}

ENTRY(_entry_point)

SECTIONS
{
    /* boot2 section - must be first */
    .boot2 : {
        __boot2_start__ = .;
        KEEP(*(.boot2))
        __boot2_end__ = .;
    } > BOOT2

    /* Vector table */
    .vectors : {
        __vectors_start__ = .;
        KEEP(*(.vectors))
        KEEP(*(.vectors.*))
        __vectors_end__ = .;
    } > FLASH

    /* Code */
    .text : {
        __text_start__ = .;
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        __text_end__ = .;
    } > FLASH

    /* ARM exception handling (can be empty for Cortex-M0+) */
    .ARM.extab : {
        *(.ARM.extab*)
    } > FLASH

    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* Initialized data - copied from flash to RAM */
    .data : {
        __data_start__ = .;
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT > FLASH
    
    __data_load_start__ = LOADADDR(.data);

    /* Uninitialized data - zeroed at startup */
    .bss (NOLOAD) : {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    /* Stack at end of RAM */
    .stack (NOLOAD) : {
        . = ALIGN(8);
        __stack_start__ = .;
        . = . + 2048;   /* 2KB stack */
        __stack_end__ = .;
        __StackTop = .;
    } > RAM

    /* Heap (minimal, mostly unused) */
    .heap (NOLOAD) : {
        __heap_start__ = .;
        . = . + 512;    /* 512 bytes heap */
        __heap_end__ = .;
    } > RAM

    /* Size calculations for startup code */
    __flash_start__ = ORIGIN(FLASH);
    __flash_end__ = ORIGIN(FLASH) + LENGTH(FLASH);
    __ram_start__ = ORIGIN(RAM);
    __ram_end__ = ORIGIN(RAM) + LENGTH(RAM);
    
    __data_size__ = __data_end__ - __data_start__;
    __bss_size__ = __bss_end__ - __bss_start__;
}

/* Provide symbols for code */
PROVIDE(_entry_point = __vectors_start__);
PROVIDE(__stack = __StackTop);
