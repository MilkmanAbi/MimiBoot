/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * handoff.c - Handoff Construction and Execution Transfer
 * 
 * This module constructs the handoff structure and performs the
 * final jump to the payload entry point.
 */

#include "handoff.h"
#include "../core/loader.h"
#include "../hal/hal.h"
#include "../../include/mimiboot/handoff.h"
#include <stddef.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Copy string with length limit (no libc).
 */
static void mimi_strncpy(char* dst, const char* src, uint32_t max) {
    uint32_t i = 0;
    while (i < max - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/**
 * Simple CRC32 (IEEE polynomial).
 * Used for optional integrity checks.
 */
static uint32_t mimi_crc32(const void* data, uint32_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}

/*============================================================================
 * Handoff Construction
 *============================================================================*/

/**
 * Build handoff structure from load result and platform info.
 */
void mimi_handoff_build(
    mimi_handoff_t*             handoff,
    const mimi_load_result_t*   load_result,
    const mimi_platform_info_t* platform,
    const char*                 image_name
) {
    /* Clear structure */
    uint8_t* p = (uint8_t*)handoff;
    for (uint32_t i = 0; i < sizeof(mimi_handoff_t); i++) {
        p[i] = 0;
    }
    
    /* Identification */
    handoff->magic = MIMI_HANDOFF_MAGIC;
    handoff->version = MIMI_HANDOFF_VERSION;
    handoff->struct_size = sizeof(mimi_handoff_t);
    
    /* Boot context - from platform */
    handoff->boot_reason = platform->reset_reason;
    handoff->boot_source = platform->boot_source;
    handoff->boot_count = 0;  /* TODO: persistent counter */
    handoff->boot_flags = 0;
    
    /* Timing */
    handoff->sys_clock_hz = platform->sys_clock_hz;
    handoff->boot_time_us = hal_get_time_us();
    handoff->loader_time_us = handoff->boot_time_us;  /* Same for now */
    
    /* Memory layout */
    handoff->ram_base = platform->ram_base;
    handoff->ram_size = platform->ram_size;
    handoff->loader_base = platform->loader_base;
    handoff->loader_size = platform->loader_size;
    
    /* Image information */
    handoff->image.entry = load_result->entry;
    handoff->image.load_base = load_result->load_base;
    handoff->image.load_size = load_result->total_size;
    handoff->image.crc32 = 0;  /* TODO: compute if requested */
    
    if (image_name != NULL) {
        mimi_strncpy(handoff->image.name, image_name, sizeof(handoff->image.name));
    }
    
    /* Memory regions */
    handoff->region_count = 0;
    
    /* Add RAM region */
    if (handoff->region_count < MIMI_MAX_REGIONS) {
        mimi_region_t* r = &handoff->regions[handoff->region_count++];
        r->base = platform->ram_base;
        r->size = platform->ram_size;
        r->flags = MIMI_REGION_RAM | MIMI_REGION_PAYLOAD;
        r->reserved = 0;
    }
    
    /* Add loader flash region */
    if (handoff->region_count < MIMI_MAX_REGIONS) {
        mimi_region_t* r = &handoff->regions[handoff->region_count++];
        r->base = platform->loader_base;
        r->size = platform->loader_size;
        r->flags = MIMI_REGION_FLASH | MIMI_REGION_LOADER;
        r->reserved = 0;
    }
    
    /* Header CRC (covers first 16 bytes) */
    handoff->header_crc = 0;  /* Clear before computing */
    handoff->header_crc = mimi_crc32(handoff, 16);
}

/*============================================================================
 * Execution Transfer
 *============================================================================*/

/**
 * Jump to payload entry point.
 * 
 * This function never returns. It:
 * 1. Disables interrupts
 * 2. Sets up the handoff pointer in r0
 * 3. Branches to the entry point
 * 
 * The payload's startup code should:
 * 1. Set up its own stack pointer
 * 2. Initialize .data and .bss
 * 3. Optionally read handoff from r0
 * 4. Continue with normal initialization
 */
__attribute__((noreturn, naked))
void mimi_handoff_jump(mimi_handoff_t* handoff, uint32_t entry) {
    /*
     * ARM Cortex-M calling convention:
     * - r0: first argument (handoff pointer)
     * - r1: second argument (entry address, but we need it in pc)
     * 
     * We want to:
     * 1. Keep handoff pointer in r0 (already there)
     * 2. Disable interrupts
     * 3. Jump to entry
     */
    
    __asm__ volatile (
        /* Disable interrupts */
        "cpsid i                    \n"
        
        /* Data synchronization barrier */
        "dsb                        \n"
        
        /* Instruction synchronization barrier */
        "isb                        \n"
        
        /*
         * r0 already contains handoff pointer
         * r1 contains entry address
         * 
         * Set LSB of entry address to 1 for Thumb mode
         * (Cortex-M only supports Thumb)
         */
        "orr r1, r1, #1             \n"
        
        /* Jump to entry point */
        "bx r1                      \n"
        
        : /* no outputs */
        : /* inputs already in registers */
        : "memory"
    );
    
    /* Never reached */
    __builtin_unreachable();
}

/**
 * Alternative jump that resets the stack pointer first.
 * 
 * Use this if the payload expects the stack pointer to be
 * set to a known value (e.g., top of RAM).
 */
__attribute__((noreturn, naked))
void mimi_handoff_jump_with_sp(mimi_handoff_t* handoff, uint32_t entry, uint32_t sp) {
    __asm__ volatile (
        /* Disable interrupts */
        "cpsid i                    \n"
        
        /* Set stack pointer from r2 */
        "msr msp, r2                \n"
        
        /* Data synchronization barrier */
        "dsb                        \n"
        
        /* Instruction synchronization barrier */  
        "isb                        \n"
        
        /* Set Thumb bit and jump */
        "orr r1, r1, #1             \n"
        "bx r1                      \n"
        
        : /* no outputs */
        : /* inputs already in registers */
        : "memory"
    );
    
    __builtin_unreachable();
}
