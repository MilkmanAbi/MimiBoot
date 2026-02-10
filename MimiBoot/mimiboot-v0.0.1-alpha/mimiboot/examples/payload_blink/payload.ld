/**
 * MimiBoot Payload Linker Script
 * 
 * For payloads that execute entirely from RAM.
 * 
 * Memory Map (RP2040):
 *   0x20000000 - 0x20041FFF : SRAM (264KB)
 * 
 * Layout:
 *   0x20000000 : Vector table (must be 256-byte aligned)
 *   0x20000100 : Code (.text)
 *   ...        : Read-only data (.rodata)
 *   ...        : Initialized data (.data)
 *   ...        : Uninitialized data (.bss)
 *   ...        : Heap (grows up)
 *   ...        : Stack (grows down from end of RAM)
 *   0x20041FFF : End of RAM
 * 
 * Note: Payloads linked with this script can use up to ~260KB
 * for code, data, heap, and stack combined.
 */

MEMORY
{
    RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

ENTRY(_entry)

SECTIONS
{
    /* Vector table at start of RAM, 256-byte aligned */
    .vectors : ALIGN(256) {
        __vectors_start__ = .;
        KEEP(*(.vectors))
        KEEP(*(.vectors.*))
        . = ALIGN(256);
        __vectors_end__ = .;
    } > RAM

    /* Entry point immediately after vectors */
    .entry : {
        KEEP(*(.entry))
        KEEP(*(.entry.*))
        . = ALIGN(4);
    } > RAM

    /* Code */
    .text : {
        __text_start__ = .;
        *(.text*)
        . = ALIGN(4);
        __text_end__ = .;
    } > RAM

    /* Read-only data */
    .rodata : {
        __rodata_start__ = .;
        *(.rodata*)
        . = ALIGN(4);
        __rodata_end__ = .;
    } > RAM

    /* ARM exception handling */
    .ARM.extab : {
        *(.ARM.extab*)
    } > RAM

    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > RAM

    /* Initialized data */
    .data : {
        __data_start__ = .;
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM

    /* Uninitialized data */
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    /* Heap starts after BSS */
    .heap (NOLOAD) : {
        . = ALIGN(8);
        __heap_start__ = .;
        __heap_base__ = .;
        /* Heap size is flexible - everything up to stack */
    } > RAM

    /* Stack at end of RAM */
    .stack (NOLOAD) : {
        . = ORIGIN(RAM) + LENGTH(RAM) - 4096;  /* 4KB stack */
        __stack_bottom__ = .;
        . = ORIGIN(RAM) + LENGTH(RAM);
        __stack_top__ = .;
    } > RAM

    /* Calculate heap end (where stack begins) */
    __heap_end__ = __stack_bottom__;
    __heap_limit__ = __stack_bottom__;
}

/* Provide symbols */
PROVIDE(__stack = __stack_top__);
PROVIDE(_estack = __stack_top__);
PROVIDE(end = __heap_start__);
PROVIDE(_end = __heap_start__);

/* Sanity checks */
ASSERT(__bss_end__ < __stack_bottom__, "BSS overlaps stack!")
ASSERT(__heap_start__ < __stack_bottom__, "Not enough space for heap and stack!")
