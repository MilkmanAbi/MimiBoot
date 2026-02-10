/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * loader.c - ELF Loader Implementation
 * 
 * Pure C implementation of ELF32 loading for ARM Cortex-M targets.
 * No external dependencies beyond standard integer types.
 * 
 * Loading Process:
 * 1. Read and validate ELF header
 * 2. Read program headers
 * 3. Validate all PT_LOAD segments fit in memory
 * 4. Copy segment data to target addresses
 * 5. Zero BSS regions (p_memsz > p_filesz)
 * 6. Return entry point and load information
 */

#include "loader.h"
#include "elf.h"
#include <stddef.h>

/*============================================================================
 * Internal Constants
 *============================================================================*/

/* Read buffer size for loading - trade-off between RAM usage and speed */
#define LOAD_BUFFER_SIZE    512

/*============================================================================
 * Internal Helpers - Memory Operations (no libc dependency)
 *============================================================================*/

/**
 * Set memory region to value (like memset but no libc).
 */
static void mimi_memset(void* dst, uint8_t val, uint32_t size) {
    uint8_t* d = (uint8_t*)dst;
    while (size--) {
        *d++ = val;
    }
}

/**
 * Copy memory (like memcpy but no libc).
 */
static void mimi_memcpy(void* dst, const void* src, uint32_t size) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (size--) {
        *d++ = *s++;
    }
}

/**
 * Compare memory (like memcmp but no libc).
 */
static int mimi_memcmp(const void* a, const void* b, uint32_t size) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    while (size--) {
        if (*pa != *pb) {
            return (*pa < *pb) ? -1 : 1;
        }
        pa++;
        pb++;
    }
    return 0;
}

/*============================================================================
 * Internal Helpers - Validation
 *============================================================================*/

/**
 * Check if address range is within a memory region.
 */
static bool mimi_addr_in_region(
    uint32_t addr,
    uint32_t size,
    const mimi_mem_region_t* region
) {
    /* Check for overflow */
    if (addr + size < addr) {
        return false;
    }
    
    uint32_t region_end = region->base + region->size;
    
    return (addr >= region->base) && ((addr + size) <= region_end);
}

/**
 * Check if address range is within any valid memory region.
 */
static bool mimi_addr_valid(
    uint32_t addr,
    uint32_t size,
    uint32_t required_flags,
    const mimi_loader_config_t* config
) {
    for (uint32_t i = 0; i < config->region_count; i++) {
        const mimi_mem_region_t* region = &config->regions[i];
        
        /* Check region has required flags */
        if ((region->flags & required_flags) != required_flags) {
            continue;
        }
        
        if (mimi_addr_in_region(addr, size, region)) {
            return true;
        }
    }
    return false;
}

/**
 * Check if two address ranges overlap.
 */
static bool mimi_ranges_overlap(
    uint32_t a_start, uint32_t a_size,
    uint32_t b_start, uint32_t b_size
) {
    uint32_t a_end = a_start + a_size;
    uint32_t b_end = b_start + b_size;
    
    return (a_start < b_end) && (b_start < a_end);
}

/*============================================================================
 * ELF Header Validation
 *============================================================================*/

mimi_err_t mimi_elf_validate_header(const Elf32_Ehdr* ehdr) {
    /* Check magic number */
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return MIMI_ERR_NOT_ELF;
    }
    
    /* Check 32-bit */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
        return MIMI_ERR_NOT_ELF32;
    }
    
    /* Check little-endian */
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return MIMI_ERR_NOT_LE;
    }
    
    /* Check ELF version */
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT || 
        ehdr->e_version != EV_CURRENT) {
        return MIMI_ERR_BAD_VERSION;
    }
    
    /* Check executable type */
    if (ehdr->e_type != ET_EXEC) {
        return MIMI_ERR_NOT_EXEC;
    }
    
    /* Check ARM architecture */
    if (ehdr->e_machine != EM_ARM) {
        return MIMI_ERR_NOT_ARM;
    }
    
    /* Check entry point exists */
    if (ehdr->e_entry == 0) {
        return MIMI_ERR_NO_ENTRY;
    }
    
    /* Check program headers exist */
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        return MIMI_ERR_NO_PHDRS;
    }
    
    /* Check program header entry size */
    if (ehdr->e_phentsize != sizeof(Elf32_Phdr)) {
        return MIMI_ERR_BAD_PHDR_SIZE;
    }
    
    /* Sanity check program header count */
    if (ehdr->e_phnum > 64) {
        return MIMI_ERR_TOO_MANY_PHDRS;
    }
    
    return MIMI_OK;
}

/*============================================================================
 * Error Strings
 *============================================================================*/

const char* mimi_strerror(mimi_err_t err) {
    switch (err) {
        case MIMI_OK:                   return "OK";
        case MIMI_ERR_IO:               return "I/O error";
        case MIMI_ERR_NOT_FOUND:        return "File not found";
        case MIMI_ERR_READ:             return "Read failed";
        case MIMI_ERR_SEEK:             return "Seek failed";
        case MIMI_ERR_NOT_ELF:          return "Not an ELF file";
        case MIMI_ERR_NOT_ELF32:        return "Not 32-bit ELF";
        case MIMI_ERR_NOT_LE:           return "Not little-endian";
        case MIMI_ERR_NOT_EXEC:         return "Not executable";
        case MIMI_ERR_NOT_ARM:          return "Not ARM architecture";
        case MIMI_ERR_BAD_VERSION:      return "Invalid ELF version";
        case MIMI_ERR_NO_ENTRY:         return "No entry point";
        case MIMI_ERR_NO_PHDRS:         return "No program headers";
        case MIMI_ERR_BAD_PHDR_SIZE:    return "Invalid program header size";
        case MIMI_ERR_TOO_MANY_PHDRS:   return "Too many program headers";
        case MIMI_ERR_NO_LOADABLE:      return "No loadable segments";
        case MIMI_ERR_ADDR_INVALID:     return "Segment address outside RAM";
        case MIMI_ERR_ADDR_OVERLAP:     return "Segments overlap";
        case MIMI_ERR_TOO_LARGE:        return "Image too large";
        case MIMI_ERR_LOAD_FAILED:      return "Load failed";
        case MIMI_ERR_ALIGNMENT:        return "Bad segment alignment";
        case MIMI_ERR_NO_MEMORY:        return "Out of memory";
        case MIMI_ERR_BAD_REGION:       return "Invalid memory region";
        default:                        return "Unknown error";
    }
}

/*============================================================================
 * Segment Loading
 *============================================================================*/

/**
 * Load a single PT_LOAD segment into memory.
 */
static mimi_err_t mimi_load_segment(
    const mimi_loader_config_t* config,
    mimi_file_t                 file,
    const Elf32_Phdr*           phdr,
    mimi_segment_info_t*        info,
    uint32_t*                   bytes_copied,
    uint32_t*                   bytes_zeroed
) {
    uint8_t buffer[LOAD_BUFFER_SIZE];
    
    /* Record segment info */
    info->vaddr = phdr->p_vaddr;
    info->size = phdr->p_memsz;
    info->flags = phdr->p_flags;
    info->loaded = false;
    
    /* Skip zero-size segments */
    if (phdr->p_memsz == 0) {
        info->loaded = true;
        return MIMI_OK;
    }
    
    /* Validate address is writable RAM */
    if (config->validate_addresses) {
        if (!mimi_addr_valid(phdr->p_vaddr, phdr->p_memsz, 
                            MIMI_MEM_WRITE | MIMI_MEM_RAM, config)) {
            return MIMI_ERR_ADDR_INVALID;
        }
    }
    
    /*
     * Load segment data from file.
     * p_filesz bytes come from the file, remaining (p_memsz - p_filesz)
     * bytes are zeroed (this is typically .bss).
     */
    
    uint32_t file_offset = phdr->p_offset;
    uint32_t dest_addr = phdr->p_vaddr;
    uint32_t remaining = phdr->p_filesz;
    
    while (remaining > 0) {
        uint32_t chunk = (remaining > LOAD_BUFFER_SIZE) ? LOAD_BUFFER_SIZE : remaining;
        
        /* Read from file */
        int32_t read_result = config->io->read(file, file_offset, buffer, chunk);
        if (read_result < 0 || (uint32_t)read_result != chunk) {
            return MIMI_ERR_READ;
        }
        
        /* Copy to target address */
        mimi_memcpy((void*)dest_addr, buffer, chunk);
        
        file_offset += chunk;
        dest_addr += chunk;
        remaining -= chunk;
        *bytes_copied += chunk;
    }
    
    /* Zero BSS portion (p_memsz > p_filesz) */
    if (config->zero_bss && phdr->p_memsz > phdr->p_filesz) {
        uint32_t bss_size = phdr->p_memsz - phdr->p_filesz;
        mimi_memset((void*)dest_addr, 0, bss_size);
        *bytes_zeroed += bss_size;
    }
    
    /* Optional verification */
    if (config->verify_after_load && phdr->p_filesz > 0) {
        file_offset = phdr->p_offset;
        dest_addr = phdr->p_vaddr;
        remaining = phdr->p_filesz;
        
        while (remaining > 0) {
            uint32_t chunk = (remaining > LOAD_BUFFER_SIZE) ? LOAD_BUFFER_SIZE : remaining;
            
            int32_t read_result = config->io->read(file, file_offset, buffer, chunk);
            if (read_result < 0 || (uint32_t)read_result != chunk) {
                return MIMI_ERR_READ;
            }
            
            if (mimi_memcmp((void*)dest_addr, buffer, chunk) != 0) {
                return MIMI_ERR_LOAD_FAILED;
            }
            
            file_offset += chunk;
            dest_addr += chunk;
            remaining -= chunk;
        }
    }
    
    info->loaded = true;
    return MIMI_OK;
}

/*============================================================================
 * Main Load Function
 *============================================================================*/

mimi_err_t mimi_elf_load(
    const mimi_loader_config_t* config,
    mimi_file_t                 file,
    mimi_load_result_t*         result
) {
    Elf32_Ehdr ehdr;
    Elf32_Phdr phdr;
    mimi_err_t err;
    
    /* Initialize result */
    mimi_memset(result, 0, sizeof(*result));
    result->load_base = 0xFFFFFFFF;
    result->load_end = 0;
    
    /*------------------------------------------------------------------------
     * Phase 1: Read and validate ELF header
     *------------------------------------------------------------------------*/
    
    int32_t read_result = config->io->read(file, 0, &ehdr, sizeof(ehdr));
    if (read_result < 0 || (uint32_t)read_result != sizeof(ehdr)) {
        result->status = MIMI_ERR_READ;
        return result->status;
    }
    
    err = mimi_elf_validate_header(&ehdr);
    if (err != MIMI_OK) {
        result->status = err;
        return result->status;
    }
    
    result->entry = ehdr.e_entry;
    
    /*------------------------------------------------------------------------
     * Phase 2: First pass - validate all segments
     *------------------------------------------------------------------------*/
    
    uint32_t loadable_count = 0;
    uint32_t total_memsz = 0;
    
    /* Temporary storage for segment info during validation */
    struct {
        uint32_t vaddr;
        uint32_t memsz;
    } seg_info[MIMI_MAX_SEGMENTS];
    
    for (uint32_t i = 0; i < ehdr.e_phnum && i < MIMI_MAX_SEGMENTS; i++) {
        uint32_t phdr_offset = ehdr.e_phoff + (i * sizeof(Elf32_Phdr));
        
        read_result = config->io->read(file, phdr_offset, &phdr, sizeof(phdr));
        if (read_result < 0 || (uint32_t)read_result != sizeof(phdr)) {
            result->status = MIMI_ERR_READ;
            return result->status;
        }
        
        /* Only process PT_LOAD segments */
        if (phdr.p_type != PT_LOAD) {
            continue;
        }
        
        /* Skip empty segments */
        if (phdr.p_memsz == 0) {
            continue;
        }
        
        /* Validate address range */
        if (config->validate_addresses) {
            if (!mimi_addr_valid(phdr.p_vaddr, phdr.p_memsz,
                                MIMI_MEM_WRITE | MIMI_MEM_RAM, config)) {
                result->status = MIMI_ERR_ADDR_INVALID;
                return result->status;
            }
        }
        
        /* Check for overlaps with previously seen segments */
        for (uint32_t j = 0; j < loadable_count; j++) {
            if (mimi_ranges_overlap(phdr.p_vaddr, phdr.p_memsz,
                                   seg_info[j].vaddr, seg_info[j].memsz)) {
                result->status = MIMI_ERR_ADDR_OVERLAP;
                return result->status;
            }
        }
        
        /* Record segment for overlap checking */
        seg_info[loadable_count].vaddr = phdr.p_vaddr;
        seg_info[loadable_count].memsz = phdr.p_memsz;
        
        /* Track memory bounds */
        if (phdr.p_vaddr < result->load_base) {
            result->load_base = phdr.p_vaddr;
        }
        if (phdr.p_vaddr + phdr.p_memsz > result->load_end) {
            result->load_end = phdr.p_vaddr + phdr.p_memsz;
        }
        
        total_memsz += phdr.p_memsz;
        loadable_count++;
    }
    
    if (loadable_count == 0) {
        result->status = MIMI_ERR_NO_LOADABLE;
        return result->status;
    }
    
    /*------------------------------------------------------------------------
     * Phase 3: Second pass - actually load segments
     *------------------------------------------------------------------------*/
    
    uint32_t seg_index = 0;
    
    for (uint32_t i = 0; i < ehdr.e_phnum; i++) {
        uint32_t phdr_offset = ehdr.e_phoff + (i * sizeof(Elf32_Phdr));
        
        read_result = config->io->read(file, phdr_offset, &phdr, sizeof(phdr));
        if (read_result < 0 || (uint32_t)read_result != sizeof(phdr)) {
            result->status = MIMI_ERR_READ;
            return result->status;
        }
        
        if (phdr.p_type != PT_LOAD) {
            continue;
        }
        
        if (seg_index >= MIMI_MAX_SEGMENTS) {
            break;
        }
        
        err = mimi_load_segment(
            config, file, &phdr,
            &result->segments[seg_index],
            &result->bytes_copied,
            &result->bytes_zeroed
        );
        
        if (err != MIMI_OK) {
            result->status = err;
            return result->status;
        }
        
        seg_index++;
    }
    
    result->segment_count = seg_index;
    result->total_size = total_memsz;
    result->status = MIMI_OK;
    
    return MIMI_OK;
}

/*============================================================================
 * Post-Load Validation
 *============================================================================*/

mimi_err_t mimi_elf_validate_loaded(const mimi_load_result_t* result) {
    if (result->status != MIMI_OK) {
        return result->status;
    }
    
    /* Check entry point is within loaded region */
    if (result->entry < result->load_base || 
        result->entry >= result->load_end) {
        return MIMI_ERR_NO_ENTRY;
    }
    
    /* Check at least one segment is executable */
    bool has_exec = false;
    for (uint32_t i = 0; i < result->segment_count; i++) {
        if (result->segments[i].flags & PF_X) {
            has_exec = true;
            break;
        }
    }
    
    if (!has_exec) {
        /* Warning but not error - some setups might not set flags */
    }
    
    return MIMI_OK;
}
