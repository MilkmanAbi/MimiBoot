/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * loader.h - ELF Loader Interface
 * 
 * Pure C, no external dependencies. Defines the loader interface
 * and all associated types.
 */

#ifndef MIMIBOOT_LOADER_H
#define MIMIBOOT_LOADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "elf.h"

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    MIMI_OK                     = 0,
    
    /* File/IO errors */
    MIMI_ERR_IO                 = -1,   /* Generic I/O error */
    MIMI_ERR_NOT_FOUND          = -2,   /* File not found */
    MIMI_ERR_READ               = -3,   /* Read failed */
    MIMI_ERR_SEEK               = -4,   /* Seek failed */
    
    /* ELF validation errors */
    MIMI_ERR_NOT_ELF            = -10,  /* Not an ELF file (bad magic) */
    MIMI_ERR_NOT_ELF32          = -11,  /* Not 32-bit ELF */
    MIMI_ERR_NOT_LE             = -12,  /* Not little-endian */
    MIMI_ERR_NOT_EXEC           = -13,  /* Not executable */
    MIMI_ERR_NOT_ARM            = -14,  /* Not ARM architecture */
    MIMI_ERR_BAD_VERSION        = -15,  /* Invalid ELF version */
    MIMI_ERR_NO_ENTRY           = -16,  /* No entry point */
    MIMI_ERR_NO_PHDRS           = -17,  /* No program headers */
    MIMI_ERR_BAD_PHDR_SIZE      = -18,  /* Invalid program header size */
    MIMI_ERR_TOO_MANY_PHDRS     = -19,  /* Too many program headers */
    
    /* Loading errors */
    MIMI_ERR_NO_LOADABLE        = -30,  /* No PT_LOAD segments */
    MIMI_ERR_ADDR_INVALID       = -31,  /* Segment address outside RAM */
    MIMI_ERR_ADDR_OVERLAP       = -32,  /* Segments overlap */
    MIMI_ERR_TOO_LARGE          = -33,  /* Image too large for RAM */
    MIMI_ERR_LOAD_FAILED        = -34,  /* Failed to load segment */
    MIMI_ERR_ALIGNMENT          = -35,  /* Bad segment alignment */
    
    /* Memory errors */
    MIMI_ERR_NO_MEMORY          = -40,  /* Out of memory */
    MIMI_ERR_BAD_REGION         = -41,  /* Invalid memory region */
    
} mimi_err_t;

/*============================================================================
 * Memory Region Description
 *============================================================================*/

/**
 * Describes a memory region available for loading.
 * The loader validates that all segments fit within defined regions.
 */
typedef struct {
    uint32_t    base;       /* Region base address */
    uint32_t    size;       /* Region size in bytes */
    uint32_t    flags;      /* Region attributes (see MIMI_MEM_*) */
} mimi_mem_region_t;

/* Memory region flags */
#define MIMI_MEM_READ       0x0001  /* Readable */
#define MIMI_MEM_WRITE      0x0002  /* Writable */
#define MIMI_MEM_EXEC       0x0004  /* Executable */
#define MIMI_MEM_RAM        0x0010  /* RAM (volatile) */
#define MIMI_MEM_FLASH      0x0020  /* Flash (non-volatile) */

/*============================================================================
 * I/O Abstraction
 *============================================================================*/

/**
 * File handle - opaque pointer to platform-specific file context.
 */
typedef void* mimi_file_t;

/**
 * I/O operations provided by the platform.
 * The loader uses these to read ELF data from storage.
 */
typedef struct {
    /**
     * Read bytes from file at specified offset.
     * 
     * @param file      File handle
     * @param offset    Byte offset from start of file
     * @param buffer    Destination buffer
     * @param size      Number of bytes to read
     * @return          Number of bytes read, or negative error
     */
    int32_t (*read)(mimi_file_t file, uint32_t offset, void* buffer, uint32_t size);
    
    /**
     * Get file size.
     * 
     * @param file      File handle
     * @return          File size in bytes, or negative error
     */
    int32_t (*size)(mimi_file_t file);
    
} mimi_io_ops_t;

/*============================================================================
 * Loaded Segment Information
 *============================================================================*/

/**
 * Information about a loaded segment.
 * Populated during loading for diagnostics and handoff.
 */
typedef struct {
    uint32_t    vaddr;      /* Virtual (load) address */
    uint32_t    size;       /* Size in memory */
    uint32_t    flags;      /* Segment flags (PF_*) */
    bool        loaded;     /* Successfully loaded */
} mimi_segment_info_t;

/* Maximum segments we track */
#define MIMI_MAX_SEGMENTS   16

/*============================================================================
 * Load Result
 *============================================================================*/

/**
 * Result of ELF loading operation.
 * Contains all information needed for handoff.
 */
typedef struct {
    /* Status */
    mimi_err_t          status;         /* Load result */
    
    /* Entry point */
    uint32_t            entry;          /* Entry point address */
    
    /* Memory usage */
    uint32_t            load_base;      /* Lowest load address */
    uint32_t            load_end;       /* Highest load address + 1 */
    uint32_t            total_size;     /* Total bytes loaded */
    
    /* Segments */
    uint32_t            segment_count;  /* Number of PT_LOAD segments */
    mimi_segment_info_t segments[MIMI_MAX_SEGMENTS];
    
    /* Statistics */
    uint32_t            bytes_copied;   /* Bytes copied from file */
    uint32_t            bytes_zeroed;   /* Bytes zeroed (BSS) */
    
} mimi_load_result_t;

/*============================================================================
 * Loader Configuration
 *============================================================================*/

/**
 * Loader configuration.
 * Defines memory constraints and behavior options.
 */
typedef struct {
    /* Memory regions available for loading */
    const mimi_mem_region_t*    regions;
    uint32_t                    region_count;
    
    /* I/O operations */
    const mimi_io_ops_t*        io;
    
    /* Options */
    bool    validate_addresses;     /* Validate segment addresses */
    bool    zero_bss;               /* Zero BSS sections */
    bool    verify_after_load;      /* Read back and verify (slow) */
    
} mimi_loader_config_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * Validate ELF header.
 * 
 * Checks that the file is a valid ELF32 little-endian ARM executable.
 * Does not read program headers or validate segments.
 * 
 * @param ehdr      Pointer to ELF header (must be at least 52 bytes)
 * @return          MIMI_OK if valid, error code otherwise
 */
mimi_err_t mimi_elf_validate_header(const Elf32_Ehdr* ehdr);

/**
 * Get human-readable error string.
 * 
 * @param err       Error code
 * @return          Static string describing the error
 */
const char* mimi_strerror(mimi_err_t err);

/**
 * Load ELF file into memory.
 * 
 * Parses the ELF file, validates it, and loads all PT_LOAD segments
 * into their specified virtual addresses. BSS sections are zeroed.
 * 
 * @param config    Loader configuration
 * @param file      File handle (passed to io ops)
 * @param result    Output: load result and segment information
 * @return          MIMI_OK if successful, error code otherwise
 */
mimi_err_t mimi_elf_load(
    const mimi_loader_config_t* config,
    mimi_file_t                 file,
    mimi_load_result_t*         result
);

/**
 * Validate loaded image.
 * 
 * Performs additional validation after loading:
 * - Checks entry point is within loaded segments
 * - Verifies segment permissions make sense
 * 
 * @param result    Load result from mimi_elf_load
 * @return          MIMI_OK if valid, error code otherwise
 */
mimi_err_t mimi_elf_validate_loaded(const mimi_load_result_t* result);

#endif /* MIMIBOOT_LOADER_H */
