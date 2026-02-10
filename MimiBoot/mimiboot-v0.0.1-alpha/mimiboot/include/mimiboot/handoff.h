/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * handoff.h - Handoff Structure Definition
 * 
 * This header defines the structure passed from MimiBoot to the payload.
 * Payloads may include this header to access boot context information.
 * 
 * The handoff structure is placed at a fixed location in memory before
 * jumping to the payload. The payload can optionally read this structure
 * to obtain information about the boot environment.
 * 
 * Usage in payload:
 * 
 *     #include <mimiboot/handoff.h>
 *     
 *     extern mimi_handoff_t __mimi_handoff;  // Linker-defined
 *     
 *     void main(void) {
 *         if (__mimi_handoff.magic == MIMI_HANDOFF_MAGIC) {
 *             // Booted via MimiBoot
 *             uint32_t sys_clock = __mimi_handoff.sys_clock_hz;
 *         }
 *     }
 * 
 * Or receive it as a parameter:
 * 
 *     void _entry(mimi_handoff_t* handoff) {
 *         if (handoff && handoff->magic == MIMI_HANDOFF_MAGIC) {
 *             // Use handoff info
 *         }
 *     }
 */

#ifndef MIMIBOOT_HANDOFF_H
#define MIMIBOOT_HANDOFF_H

#include <stdint.h>

/*============================================================================
 * Version and Magic
 *============================================================================*/

/**
 * Handoff magic number: 'MIMI' in little-endian
 */
#define MIMI_HANDOFF_MAGIC      0x494D494D

/**
 * Handoff structure version.
 * Increment when structure changes in incompatible ways.
 */
#define MIMI_HANDOFF_VERSION    1

/*============================================================================
 * Boot Reason Flags
 *============================================================================*/

#define MIMI_BOOT_COLD          0x00000001  /* Power-on reset */
#define MIMI_BOOT_WARM          0x00000002  /* Software-triggered reset */
#define MIMI_BOOT_WATCHDOG      0x00000004  /* Watchdog timeout reset */
#define MIMI_BOOT_BROWNOUT      0x00000008  /* Brownout/low voltage reset */
#define MIMI_BOOT_EXTERNAL      0x00000010  /* External reset pin */
#define MIMI_BOOT_DEBUG         0x00000020  /* Debug/JTAG reset */
#define MIMI_BOOT_UNKNOWN       0x80000000  /* Unknown reset cause */

/*============================================================================
 * Boot Source Flags
 *============================================================================*/

#define MIMI_SOURCE_SD          0x00000001  /* SD card (SPI mode) */
#define MIMI_SOURCE_SDIO        0x00000002  /* SD card (SDIO mode) */
#define MIMI_SOURCE_SPI_FLASH   0x00000004  /* SPI NOR flash */
#define MIMI_SOURCE_QSPI_FLASH  0x00000008  /* QSPI flash */
#define MIMI_SOURCE_UART        0x00000010  /* UART download */
#define MIMI_SOURCE_USB         0x00000020  /* USB download */
#define MIMI_SOURCE_INTERNAL    0x00000040  /* Internal flash (fallback) */

/*============================================================================
 * Memory Region Description
 *============================================================================*/

/**
 * Memory region flags for handoff.
 */
#define MIMI_REGION_RAM         0x00000001  /* General-purpose RAM */
#define MIMI_REGION_FLASH       0x00000002  /* Flash memory */
#define MIMI_REGION_PERIPHERAL  0x00000004  /* Memory-mapped peripherals */
#define MIMI_REGION_LOADER      0x00000010  /* MimiBoot resides here */
#define MIMI_REGION_PAYLOAD     0x00000020  /* Payload loaded here */
#define MIMI_REGION_HANDOFF     0x00000040  /* Handoff struct here */
#define MIMI_REGION_RESERVED    0x00000080  /* Reserved, do not use */

/**
 * Memory region descriptor.
 */
typedef struct {
    uint32_t    base;       /* Region base address */
    uint32_t    size;       /* Region size in bytes */
    uint32_t    flags;      /* Region flags (MIMI_REGION_*) */
    uint32_t    reserved;   /* Reserved for alignment */
} mimi_region_t;

/**
 * Maximum number of memory regions in handoff.
 */
#define MIMI_MAX_REGIONS        8

/*============================================================================
 * Image Information
 *============================================================================*/

/**
 * Information about the loaded image.
 */
typedef struct {
    uint32_t    entry;          /* Entry point address */
    uint32_t    load_base;      /* Lowest load address */
    uint32_t    load_size;      /* Total size in memory */
    uint32_t    crc32;          /* CRC32 of loaded data (0 if not computed) */
    char        name[32];       /* Image filename (null-terminated) */
} mimi_image_info_t;

/*============================================================================
 * Main Handoff Structure
 *============================================================================*/

/**
 * MimiBoot Handoff Structure
 * 
 * Passed to the payload at entry. Contains all context the payload
 * might need about the boot environment.
 * 
 * Size: Fixed at 256 bytes for predictable placement.
 */
typedef struct {
    /*--- Identification (offset 0x00) ---*/
    uint32_t    magic;              /* MIMI_HANDOFF_MAGIC */
    uint32_t    version;            /* MIMI_HANDOFF_VERSION */
    uint32_t    struct_size;        /* sizeof(mimi_handoff_t) */
    uint32_t    header_crc;         /* CRC32 of header (0 if not computed) */
    
    /*--- Boot Context (offset 0x10) ---*/
    uint32_t    boot_reason;        /* Reset cause (MIMI_BOOT_*) */
    uint32_t    boot_source;        /* Image source (MIMI_SOURCE_*) */
    uint32_t    boot_count;         /* Boot attempt counter */
    uint32_t    boot_flags;         /* Additional flags */
    
    /*--- Timing (offset 0x20) ---*/
    uint32_t    sys_clock_hz;       /* System clock frequency */
    uint32_t    boot_time_us;       /* Microseconds from reset to handoff */
    uint32_t    loader_time_us;     /* Time spent in loader */
    uint32_t    reserved_timing;    /* Reserved */
    
    /*--- Memory Layout (offset 0x30) ---*/
    uint32_t    ram_base;           /* Primary RAM base */
    uint32_t    ram_size;           /* Primary RAM size */
    uint32_t    loader_base;        /* MimiBoot location in flash */
    uint32_t    loader_size;        /* MimiBoot size */
    
    /*--- Image Info (offset 0x40) ---*/
    mimi_image_info_t image;        /* Loaded image information (48 bytes) */
    
    /*--- Memory Regions (offset 0x70) ---*/
    uint32_t    region_count;       /* Number of valid regions */
    uint32_t    reserved_regions;   /* Reserved */
    mimi_region_t regions[MIMI_MAX_REGIONS];  /* Memory region descriptors */
    
    /*--- Reserved for future use ---*/
    uint32_t    reserved[8];
    
} mimi_handoff_t;

/*============================================================================
 * Compile-Time Validation
 *============================================================================*/

/**
 * Ensure structure is exactly 256 bytes.
 * This is important for predictable memory layout.
 */
_Static_assert(sizeof(mimi_handoff_t) == 256, 
    "mimi_handoff_t must be exactly 256 bytes");

/*============================================================================
 * Helper Macros
 *============================================================================*/

/**
 * Check if handoff structure is valid.
 */
#define MIMI_HANDOFF_VALID(h) \
    ((h) != NULL && \
     (h)->magic == MIMI_HANDOFF_MAGIC && \
     (h)->version == MIMI_HANDOFF_VERSION)

/**
 * Get end of RAM from handoff.
 */
#define MIMI_RAM_END(h) \
    ((h)->ram_base + (h)->ram_size)

/**
 * Default handoff location: end of RAM minus structure size, aligned.
 * Platform-specific code should define MIMI_HANDOFF_ADDR appropriately.
 */
#ifndef MIMI_HANDOFF_ADDR
#define MIMI_HANDOFF_ADDR(ram_end) \
    (((ram_end) - sizeof(mimi_handoff_t)) & ~0xFF)
#endif

#endif /* MIMIBOOT_HANDOFF_H */
