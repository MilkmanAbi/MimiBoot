/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * elf.h - ELF32 Structure Definitions
 * 
 * Pure C, no external dependencies. These structures match the ELF32
 * specification exactly for little-endian ARM targets.
 * 
 * Reference: Tool Interface Standard (TIS) Executable and Linking Format (ELF)
 *            Specification Version 1.2
 */

#ifndef MIMIBOOT_ELF_H
#define MIMIBOOT_ELF_H

#include <stdint.h>

/*============================================================================
 * ELF Identification Constants
 *============================================================================*/

/* e_ident[] indices */
#define EI_MAG0         0       /* File identification byte 0 */
#define EI_MAG1         1       /* File identification byte 1 */
#define EI_MAG2         2       /* File identification byte 2 */
#define EI_MAG3         3       /* File identification byte 3 */
#define EI_CLASS        4       /* File class */
#define EI_DATA         5       /* Data encoding */
#define EI_VERSION      6       /* File version */
#define EI_OSABI        7       /* OS/ABI identification */
#define EI_ABIVERSION   8       /* ABI version */
#define EI_PAD          9       /* Start of padding bytes */
#define EI_NIDENT       16      /* Size of e_ident[] */

/* Magic number */
#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'

/* e_ident[EI_CLASS] - File class */
#define ELFCLASSNONE    0       /* Invalid class */
#define ELFCLASS32      1       /* 32-bit objects */
#define ELFCLASS64      2       /* 64-bit objects */

/* e_ident[EI_DATA] - Data encoding */
#define ELFDATANONE     0       /* Invalid data encoding */
#define ELFDATA2LSB     1       /* Little-endian */
#define ELFDATA2MSB     2       /* Big-endian */

/* e_ident[EI_VERSION] and e_version */
#define EV_NONE         0       /* Invalid version */
#define EV_CURRENT      1       /* Current version */

/*============================================================================
 * ELF Header Constants
 *============================================================================*/

/* e_type - Object file type */
#define ET_NONE         0       /* No file type */
#define ET_REL          1       /* Relocatable file */
#define ET_EXEC         2       /* Executable file */
#define ET_DYN          3       /* Shared object file */
#define ET_CORE         4       /* Core file */

/* e_machine - Architecture */
#define EM_NONE         0       /* No machine */
#define EM_ARM          40      /* ARM 32-bit architecture */

/* ARM-specific e_flags */
#define EF_ARM_EABI_MASK    0xFF000000
#define EF_ARM_EABI_VER5    0x05000000

/*============================================================================
 * Program Header Constants
 *============================================================================*/

/* p_type - Segment type */
#define PT_NULL         0       /* Unused entry */
#define PT_LOAD         1       /* Loadable segment */
#define PT_DYNAMIC      2       /* Dynamic linking information */
#define PT_INTERP       3       /* Interpreter pathname */
#define PT_NOTE         4       /* Auxiliary information */
#define PT_SHLIB        5       /* Reserved */
#define PT_PHDR         6       /* Program header table */
#define PT_TLS          7       /* Thread-local storage */

/* ARM-specific segment types */
#define PT_ARM_EXIDX    0x70000001  /* ARM exception index table */

/* p_flags - Segment flags */
#define PF_X            0x1     /* Execute permission */
#define PF_W            0x2     /* Write permission */
#define PF_R            0x4     /* Read permission */

/*============================================================================
 * Section Header Constants (for reference, not used in loading)
 *============================================================================*/

/* sh_type - Section type */
#define SHT_NULL        0       /* Inactive section */
#define SHT_PROGBITS    1       /* Program-defined information */
#define SHT_SYMTAB      2       /* Symbol table */
#define SHT_STRTAB      3       /* String table */
#define SHT_RELA        4       /* Relocation entries with addends */
#define SHT_HASH        5       /* Symbol hash table */
#define SHT_DYNAMIC     6       /* Dynamic linking information */
#define SHT_NOTE        7       /* Notes */
#define SHT_NOBITS      8       /* Occupies no space in file (BSS) */
#define SHT_REL         9       /* Relocation entries without addends */

/*============================================================================
 * ELF32 Structures
 *============================================================================*/

/**
 * ELF32 File Header
 * 
 * Located at offset 0 in the ELF file. Contains file identification,
 * entry point, and offsets to program/section header tables.
 */
typedef struct {
    uint8_t     e_ident[EI_NIDENT]; /* ELF identification */
    uint16_t    e_type;             /* Object file type */
    uint16_t    e_machine;          /* Machine architecture */
    uint32_t    e_version;          /* Object file version */
    uint32_t    e_entry;            /* Entry point virtual address */
    uint32_t    e_phoff;            /* Program header table file offset */
    uint32_t    e_shoff;            /* Section header table file offset */
    uint32_t    e_flags;            /* Processor-specific flags */
    uint16_t    e_ehsize;           /* ELF header size */
    uint16_t    e_phentsize;        /* Program header table entry size */
    uint16_t    e_phnum;            /* Program header table entry count */
    uint16_t    e_shentsize;        /* Section header table entry size */
    uint16_t    e_shnum;            /* Section header table entry count */
    uint16_t    e_shstrndx;         /* Section name string table index */
} Elf32_Ehdr;

/**
 * ELF32 Program Header
 * 
 * Describes a segment to be loaded into memory. The loader iterates
 * all program headers and processes PT_LOAD entries.
 */
typedef struct {
    uint32_t    p_type;     /* Segment type */
    uint32_t    p_offset;   /* Segment file offset */
    uint32_t    p_vaddr;    /* Segment virtual address */
    uint32_t    p_paddr;    /* Segment physical address (unused on Cortex-M) */
    uint32_t    p_filesz;   /* Segment size in file */
    uint32_t    p_memsz;    /* Segment size in memory */
    uint32_t    p_flags;    /* Segment flags (R/W/X) */
    uint32_t    p_align;    /* Segment alignment */
} Elf32_Phdr;

/**
 * ELF32 Section Header (for reference)
 * 
 * MimiBoot does not process section headers during loading.
 * Program headers contain all information needed to load the image.
 */
typedef struct {
    uint32_t    sh_name;        /* Section name (string table index) */
    uint32_t    sh_type;        /* Section type */
    uint32_t    sh_flags;       /* Section flags */
    uint32_t    sh_addr;        /* Section virtual address */
    uint32_t    sh_offset;      /* Section file offset */
    uint32_t    sh_size;        /* Section size in bytes */
    uint32_t    sh_link;        /* Link to another section */
    uint32_t    sh_info;        /* Additional section information */
    uint32_t    sh_addralign;   /* Section alignment */
    uint32_t    sh_entsize;     /* Entry size if section holds table */
} Elf32_Shdr;

/*============================================================================
 * Compile-Time Validation
 *============================================================================*/

/* Ensure structures are packed correctly (no padding) */
_Static_assert(sizeof(Elf32_Ehdr) == 52, "Elf32_Ehdr size mismatch");
_Static_assert(sizeof(Elf32_Phdr) == 32, "Elf32_Phdr size mismatch");
_Static_assert(sizeof(Elf32_Shdr) == 40, "Elf32_Shdr size mismatch");

/*============================================================================
 * Helper Macros
 *============================================================================*/

/**
 * Validate ELF magic number
 */
#define ELF_MAGIC_VALID(ehdr) \
    ((ehdr)->e_ident[EI_MAG0] == ELFMAG0 && \
     (ehdr)->e_ident[EI_MAG1] == ELFMAG1 && \
     (ehdr)->e_ident[EI_MAG2] == ELFMAG2 && \
     (ehdr)->e_ident[EI_MAG3] == ELFMAG3)

/**
 * Check if ELF is 32-bit little-endian ARM executable
 */
#define ELF_IS_VALID_ARM32(ehdr) \
    (ELF_MAGIC_VALID(ehdr) && \
     (ehdr)->e_ident[EI_CLASS] == ELFCLASS32 && \
     (ehdr)->e_ident[EI_DATA] == ELFDATA2LSB && \
     (ehdr)->e_type == ET_EXEC && \
     (ehdr)->e_machine == EM_ARM)

#endif /* MIMIBOOT_ELF_H */
