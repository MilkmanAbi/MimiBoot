/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * config.h - Boot Configuration Interface
 * 
 * Parses boot.cfg from the root of the SD card to determine
 * which image to load and with what parameters.
 * 
 * Configuration file format (boot.cfg):
 * 
 *     # MimiBoot Configuration
 *     image = /boot/kernel.elf
 *     timeout = 3
 *     fallback = /boot/recovery.elf
 *     console = uart0
 *     baudrate = 115200
 *     verbose = 1
 * 
 * Simple key=value format, # for comments, whitespace ignored.
 */

#ifndef MIMIBOOT_CONFIG_H
#define MIMIBOOT_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * Constants
 *============================================================================*/

#define CONFIG_MAX_PATH         128
#define CONFIG_MAX_LINE         256
#define CONFIG_MAX_IMAGES       8

/*============================================================================
 * Boot Image Entry
 *============================================================================*/

typedef struct {
    char        path[CONFIG_MAX_PATH];  /* Path to ELF file */
    char        name[32];               /* Display name */
    uint32_t    flags;                  /* Image flags */
    bool        valid;                  /* Entry is valid */
} mimi_image_entry_t;

/* Image flags */
#define IMAGE_FLAG_DEFAULT      0x0001  /* Default boot image */
#define IMAGE_FLAG_FALLBACK     0x0002  /* Fallback/recovery image */
#define IMAGE_FLAG_ONCE         0x0004  /* Boot once, then revert */

/*============================================================================
 * Boot Configuration
 *============================================================================*/

typedef struct {
    /* Primary image */
    char        image_path[CONFIG_MAX_PATH];
    
    /* Fallback image */
    char        fallback_path[CONFIG_MAX_PATH];
    bool        has_fallback;
    
    /* Boot menu (optional) */
    mimi_image_entry_t  images[CONFIG_MAX_IMAGES];
    uint32_t            image_count;
    uint32_t            default_index;
    
    /* Timing */
    uint32_t    timeout_ms;         /* Menu timeout (0 = no menu) */
    uint32_t    boot_delay_ms;      /* Delay before boot */
    
    /* Console */
    uint32_t    console_baud;       /* UART baud rate */
    bool        verbose;            /* Verbose boot messages */
    bool        quiet;              /* Suppress all output */
    
    /* Options */
    bool        verify;             /* Verify loaded image */
    bool        reset_on_fail;      /* Reset on boot failure */
    uint32_t    max_retries;        /* Max boot attempts */
    
    /* State */
    uint32_t    boot_count;         /* Current boot attempt */
    bool        config_loaded;      /* Config file was found */
    
} mimi_config_t;

/*============================================================================
 * Default Configuration
 *============================================================================*/

#define MIMI_DEFAULT_IMAGE      "/boot/kernel.elf"
#define MIMI_DEFAULT_FALLBACK   "/boot/recovery.elf"
#define MIMI_DEFAULT_CONFIG     "/boot.cfg"
#define MIMI_DEFAULT_TIMEOUT    0       /* No menu by default */
#define MIMI_DEFAULT_BAUD       115200
#define MIMI_DEFAULT_VERBOSE    true

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * Initialize configuration with defaults.
 * 
 * @param config    Configuration structure to initialize
 */
void mimi_config_init(mimi_config_t* config);

/**
 * Load configuration from file.
 * 
 * @param config    Configuration structure to populate
 * @param read_file Function to read file contents
 * @param path      Path to configuration file
 * @return          0 on success, negative on error
 */
int mimi_config_load(
    mimi_config_t* config,
    int (*read_file)(const char* path, char* buffer, uint32_t max_size),
    const char* path
);

/**
 * Parse configuration from buffer.
 * 
 * @param config    Configuration structure to populate
 * @param buffer    Null-terminated configuration text
 * @return          0 on success, negative on error
 */
int mimi_config_parse(mimi_config_t* config, const char* buffer);

/**
 * Get path to boot image.
 * 
 * Returns primary image path, or fallback if primary failed
 * and retries exceeded.
 * 
 * @param config    Configuration
 * @return          Path to image, or NULL if none available
 */
const char* mimi_config_get_image(mimi_config_t* config);

/**
 * Mark boot attempt.
 * 
 * Increments boot counter. Called before attempting to load.
 * 
 * @param config    Configuration
 */
void mimi_config_boot_attempt(mimi_config_t* config);

/**
 * Mark boot success.
 * 
 * Resets boot counter. Called after successful handoff
 * (though we never return, this could be stored persistently).
 * 
 * @param config    Configuration
 */
void mimi_config_boot_success(mimi_config_t* config);

#endif /* MIMIBOOT_CONFIG_H */
