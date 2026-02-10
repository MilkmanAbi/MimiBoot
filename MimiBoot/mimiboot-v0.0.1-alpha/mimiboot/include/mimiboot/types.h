/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * types.h - Common Type Definitions
 * 
 * This header provides portable type definitions for all MimiBoot code.
 * Uses stdint.h types for explicit sizing.
 */

#ifndef MIMIBOOT_TYPES_H
#define MIMIBOOT_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*============================================================================
 * Version Information
 *============================================================================*/

#define MIMIBOOT_VERSION_MAJOR  0
#define MIMIBOOT_VERSION_MINOR  0
#define MIMIBOOT_VERSION_PATCH  1

#define MIMIBOOT_VERSION \
    ((MIMIBOOT_VERSION_MAJOR << 16) | \
     (MIMIBOOT_VERSION_MINOR << 8) | \
     MIMIBOOT_VERSION_PATCH)

#define MIMIBOOT_VERSION_STRING "0.0.1-alpha"

/*============================================================================
 * Build Configuration
 *============================================================================*/

/* Target platform detection */
#if defined(TARGET_RP2040)
    #define MIMIBOOT_PLATFORM_RP2040    1
    #define MIMIBOOT_PLATFORM_NAME      "RP2040"
#elif defined(TARGET_RP2350)
    #define MIMIBOOT_PLATFORM_RP2350    1
    #define MIMIBOOT_PLATFORM_NAME      "RP2350"
#elif defined(TARGET_STM32F4)
    #define MIMIBOOT_PLATFORM_STM32F4   1
    #define MIMIBOOT_PLATFORM_NAME      "STM32F4"
#else
    #define MIMIBOOT_PLATFORM_GENERIC   1
    #define MIMIBOOT_PLATFORM_NAME      "Generic"
#endif

/* Compiler detection */
#if defined(__GNUC__)
    #define MIMIBOOT_COMPILER_GCC       1
    #define MIMIBOOT_PACKED             __attribute__((packed))
    #define MIMIBOOT_ALIGNED(n)         __attribute__((aligned(n)))
    #define MIMIBOOT_NORETURN           __attribute__((noreturn))
    #define MIMIBOOT_WEAK               __attribute__((weak))
    #define MIMIBOOT_USED               __attribute__((used))
    #define MIMIBOOT_SECTION(s)         __attribute__((section(s)))
    #define MIMIBOOT_INLINE             __attribute__((always_inline)) inline
    #define MIMIBOOT_NOINLINE           __attribute__((noinline))
#else
    #define MIMIBOOT_PACKED
    #define MIMIBOOT_ALIGNED(n)
    #define MIMIBOOT_NORETURN
    #define MIMIBOOT_WEAK
    #define MIMIBOOT_USED
    #define MIMIBOOT_SECTION(s)
    #define MIMIBOOT_INLINE             inline
    #define MIMIBOOT_NOINLINE
#endif

/*============================================================================
 * Utility Macros
 *============================================================================*/

/* Array size */
#define MIMIBOOT_ARRAY_SIZE(arr)    (sizeof(arr) / sizeof((arr)[0]))

/* Min/Max */
#define MIMIBOOT_MIN(a, b)          (((a) < (b)) ? (a) : (b))
#define MIMIBOOT_MAX(a, b)          (((a) > (b)) ? (a) : (b))

/* Alignment */
#define MIMIBOOT_ALIGN_UP(val, align) \
    (((val) + (align) - 1) & ~((align) - 1))
#define MIMIBOOT_ALIGN_DOWN(val, align) \
    ((val) & ~((align) - 1))

/* Check if aligned */
#define MIMIBOOT_IS_ALIGNED(val, align) \
    (((val) & ((align) - 1)) == 0)

/* Bit manipulation */
#define MIMIBOOT_BIT(n)             (1U << (n))
#define MIMIBOOT_BIT_SET(val, n)    ((val) | MIMIBOOT_BIT(n))
#define MIMIBOOT_BIT_CLR(val, n)    ((val) & ~MIMIBOOT_BIT(n))
#define MIMIBOOT_BIT_TST(val, n)    (((val) & MIMIBOOT_BIT(n)) != 0)

/* Memory barrier */
#if defined(__GNUC__)
    #define MIMIBOOT_DSB()          __asm__ volatile ("dsb" ::: "memory")
    #define MIMIBOOT_ISB()          __asm__ volatile ("isb" ::: "memory")
    #define MIMIBOOT_DMB()          __asm__ volatile ("dmb" ::: "memory")
#else
    #define MIMIBOOT_DSB()
    #define MIMIBOOT_ISB()
    #define MIMIBOOT_DMB()
#endif

#endif /* MIMIBOOT_TYPES_H */
