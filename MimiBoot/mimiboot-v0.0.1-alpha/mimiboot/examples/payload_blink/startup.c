/**
 * MimiBoot Example Payload - Startup Code
 * 
 * Minimal startup for RAM-loaded payloads.
 * 
 * When MimiBoot jumps to us:
 * - r0 contains pointer to handoff structure
 * - We're running from RAM
 * - Interrupts are disabled
 * - We need to set up our own stack
 */

#include <stdint.h>

/* Linker symbols */
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;
extern uint32_t __stack_top__;

/* Main function */
extern void payload_main(void* handoff);

/**
 * Reset handler / Entry point.
 * 
 * MimiBoot jumps here with handoff pointer in r0.
 */
__attribute__((naked, section(".entry")))
void _entry(void) {
    __asm__ volatile (
        /* Save handoff pointer (r0) */
        "mov r4, r0                 \n"
        
        /* Set up stack pointer */
        "ldr r0, =__stack_top__     \n"
        "mov sp, r0                 \n"
        
        /* Zero BSS */
        "ldr r0, =__bss_start__     \n"
        "ldr r1, =__bss_end__       \n"
        "movs r2, #0                \n"
        "1:                         \n"
        "cmp r0, r1                 \n"
        "bge 2f                     \n"
        "str r2, [r0]               \n"
        "adds r0, #4                \n"
        "b 1b                       \n"
        "2:                         \n"
        
        /* Restore handoff pointer and call main */
        "mov r0, r4                 \n"
        "bl payload_main            \n"
        
        /* If main returns, hang */
        "3:                         \n"
        "wfi                        \n"
        "b 3b                       \n"
    );
}

/**
 * Vector table for RAM execution.
 * 
 * Cortex-M requires vectors at specific locations.
 * We relocate VTOR to point here after loading.
 */
__attribute__((section(".vectors"), used))
const uint32_t vector_table[] = {
    (uint32_t)&__stack_top__,   /* Initial SP */
    (uint32_t)_entry,           /* Reset handler */
    (uint32_t)_entry,           /* NMI - just reset */
    (uint32_t)_entry,           /* HardFault - just reset */
    /* Add more vectors as needed... */
};

/**
 * Default handler for unused interrupts.
 */
__attribute__((weak))
void Default_Handler(void) {
    while (1) {
        __asm__ volatile ("wfi");
    }
}
