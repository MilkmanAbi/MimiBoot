/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * handoff.h (internal) - Handoff construction interface
 */

#ifndef MIMIBOOT_CORE_HANDOFF_H
#define MIMIBOOT_CORE_HANDOFF_H

#include "../../include/mimiboot/handoff.h"
#include "loader.h"
#include "../hal/hal.h"

/**
 * Build handoff structure from load result and platform info.
 * 
 * @param handoff       Output handoff structure
 * @param load_result   Result from ELF loader
 * @param platform      Platform information
 * @param image_name    Name of loaded image (optional, can be NULL)
 */
void mimi_handoff_build(
    mimi_handoff_t*             handoff,
    const mimi_load_result_t*   load_result,
    const mimi_platform_info_t* platform,
    const char*                 image_name
);

/**
 * Jump to payload entry point.
 * 
 * Passes handoff pointer in r0 and transfers execution to entry.
 * This function never returns.
 * 
 * @param handoff   Handoff structure pointer (passed in r0)
 * @param entry     Payload entry point address
 */
__attribute__((noreturn))
void mimi_handoff_jump(mimi_handoff_t* handoff, uint32_t entry);

/**
 * Jump to payload with explicit stack pointer.
 * 
 * Sets MSP to sp before jumping. Use when payload expects
 * stack pointer at a specific location.
 * 
 * @param handoff   Handoff structure pointer
 * @param entry     Payload entry point address
 * @param sp        Initial stack pointer value
 */
__attribute__((noreturn))
void mimi_handoff_jump_with_sp(mimi_handoff_t* handoff, uint32_t entry, uint32_t sp);

#endif /* MIMIBOOT_CORE_HANDOFF_H */
