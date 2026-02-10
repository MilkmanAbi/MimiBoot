/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * main.c - Bootloader Entry Point
 * 
 * This is where it all comes together. The boot sequence is:
 * 
 * 1. Early hardware init (clocks, GPIO)
 * 2. Console init (UART for debug output)
 * 3. Storage init (SD card over SPI)
 * 4. Mount filesystem (FAT32)
 * 5. Load configuration (boot.cfg)
 * 6. Load ELF image into RAM
 * 7. Build handoff structure
 * 8. Jump to payload
 * 
 * If anything fails, we either retry, load fallback, or halt with
 * an error indication (LED blink pattern).
 */

#include "core/loader.h"
#include "core/config.h"
#include "core/handoff.h"
#include "hal/hal.h"
#include "fs/fat32.h"
#include "../include/mimiboot/handoff.h"

/*============================================================================
 * Version Information
 *============================================================================*/

#define MIMIBOOT_VERSION_MAJOR  0
#define MIMIBOOT_VERSION_MINOR  0
#define MIMIBOOT_VERSION_PATCH  1
#define MIMIBOOT_VERSION_STRING "0.0.1-alpha"

/*============================================================================
 * Error LED Patterns
 *============================================================================*/

#define BLINK_INIT_FAIL         2   /* Hardware init failed */
#define BLINK_STORAGE_FAIL      3   /* SD card not found */
#define BLINK_FS_FAIL           4   /* FAT32 mount failed */
#define BLINK_FILE_NOT_FOUND    5   /* ELF file not found */
#define BLINK_ELF_INVALID       6   /* ELF validation failed */
#define BLINK_LOAD_FAIL         7   /* ELF loading failed */
#define BLINK_NO_MEMORY         8   /* Image too large for RAM */

/*============================================================================
 * Static State
 *============================================================================*/

static fat32_fs_t       s_fs;
static mimi_config_t    s_config;
static mimi_handoff_t   s_handoff __attribute__((aligned(256)));

/*============================================================================
 * Logging
 *============================================================================*/

#define LOG(fmt, ...) \
    do { \
        if (!s_config.quiet) { \
            hal_console_printf(fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_VERBOSE(fmt, ...) \
    do { \
        if (s_config.verbose && !s_config.quiet) { \
            hal_console_printf(fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/*============================================================================
 * Filesystem Callbacks
 *============================================================================*/

/* Global storage handle for callbacks */
static hal_storage_t s_storage;

/**
 * Sector read callback for FAT32 driver.
 */
static int fs_read_sector(uint32_t sector, uint8_t* buffer) {
    int32_t result = hal_storage_read(s_storage, sector * 512, buffer, 512);
    return (result == 512) ? 0 : -1;
}

/**
 * File read callback for config loader.
 */
static int config_read_file(const char* path, char* buffer, uint32_t max_size) {
    fat32_file_t file;
    
    fat32_err_t err = fat32_open(&s_fs, path, &file);
    if (err != FAT32_OK) {
        return -1;
    }
    
    uint32_t size = fat32_size(&file);
    if (size > max_size - 1) {
        size = max_size - 1;
    }
    
    int32_t read = fat32_read(&file, buffer, size);
    if (read < 0) {
        return read;
    }
    
    buffer[read] = '\0';
    return read;
}

/*============================================================================
 * I/O Operations for ELF Loader
 *============================================================================*/

typedef struct {
    fat32_file_t file;
} loader_file_ctx_t;

static loader_file_ctx_t s_loader_ctx;

static int32_t loader_io_read(mimi_file_t file, uint32_t offset, void* buffer, uint32_t size) {
    loader_file_ctx_t* ctx = (loader_file_ctx_t*)file;
    
    fat32_err_t err = fat32_seek(&ctx->file, offset);
    if (err != FAT32_OK) {
        return -1;
    }
    
    return fat32_read(&ctx->file, buffer, size);
}

static int32_t loader_io_size(mimi_file_t file) {
    loader_file_ctx_t* ctx = (loader_file_ctx_t*)file;
    return fat32_size(&ctx->file);
}

static const mimi_io_ops_t s_loader_io = {
    .read = loader_io_read,
    .size = loader_io_size,
};

/*============================================================================
 * Boot Failure Handler
 *============================================================================*/

__attribute__((noreturn))
static void boot_fail(int blink_code, const char* message) {
    LOG("\n[FAIL] %s\n", message);
    LOG("Blink code: %d\n", blink_code);
    
    /* Blink error pattern forever */
    while (1) {
        hal_led_blink(blink_code, 200, 200);
        hal_delay_ms(1000);
    }
}

/*============================================================================
 * Main Boot Sequence
 *============================================================================*/

int main(void) {
    mimi_err_t err;
    mimi_platform_info_t platform;
    mimi_load_result_t load_result;
    uint32_t boot_start_us;
    
    /*------------------------------------------------------------------------
     * Phase 1: Early Hardware Initialization
     *------------------------------------------------------------------------*/
    
    if (hal_init_early() != 0) {
        /* Can't even blink LED properly here, just halt */
        while (1) {}
    }
    
    boot_start_us = hal_get_time_us();
    
    /* Initialize configuration with defaults */
    mimi_config_init(&s_config);
    
    /* Initialize console */
    hal_console_init();
    
    /* Get platform information */
    hal_get_platform_info(&platform);
    
    /*------------------------------------------------------------------------
     * Phase 2: Banner and System Info
     *------------------------------------------------------------------------*/
    
    LOG("\n");
    LOG("========================================\n");
    LOG("  MimiBoot v%s\n", MIMIBOOT_VERSION_STRING);
    LOG("  Minimal ELF Bootloader for ARM Cortex-M\n");
    LOG("========================================\n");
    LOG("\n");
    
    LOG_VERBOSE("Platform: %s\n", platform.platform_name);
    LOG_VERBOSE("RAM: 0x%08X - 0x%08X (%u KB)\n", 
        platform.ram_base, 
        platform.ram_base + platform.ram_size,
        platform.ram_size / 1024);
    LOG_VERBOSE("Clock: %u MHz\n", platform.sys_clock_hz / 1000000);
    LOG_VERBOSE("\n");
    
    /*------------------------------------------------------------------------
     * Phase 3: Storage Initialization
     *------------------------------------------------------------------------*/
    
    LOG("Initializing storage...\n");
    
    if (hal_storage_init() != 0) {
        boot_fail(BLINK_STORAGE_FAIL, "Storage init failed");
    }
    
    if (hal_storage_open(&s_storage) != 0) {
        boot_fail(BLINK_STORAGE_FAIL, "SD card not found");
    }
    
    hal_storage_info_t storage_info;
    hal_storage_info(s_storage, &storage_info);
    
    LOG_VERBOSE("Storage: %s\n", storage_info.name);
    LOG_VERBOSE("Capacity: %u MB\n", storage_info.total_size / (1024 * 1024));
    
    /*------------------------------------------------------------------------
     * Phase 4: Mount Filesystem
     *------------------------------------------------------------------------*/
    
    LOG("Mounting filesystem...\n");
    
    fat32_err_t fs_err = fat32_mount(&s_fs, fs_read_sector);
    if (fs_err != FAT32_OK) {
        boot_fail(BLINK_FS_FAIL, "FAT32 mount failed");
    }
    
    LOG_VERBOSE("Filesystem mounted\n");
    LOG_VERBOSE("Cluster size: %u bytes\n", s_fs.cluster_size);
    
    /*------------------------------------------------------------------------
     * Phase 5: Load Configuration
     *------------------------------------------------------------------------*/
    
    LOG("Loading configuration...\n");
    
    int cfg_result = mimi_config_load(&s_config, config_read_file, MIMI_DEFAULT_CONFIG);
    
    if (cfg_result < 0) {
        LOG_VERBOSE("No boot.cfg found, using defaults\n");
    } else {
        LOG_VERBOSE("Configuration loaded\n");
    }
    
    LOG_VERBOSE("Boot image: %s\n", s_config.image_path);
    if (s_config.has_fallback) {
        LOG_VERBOSE("Fallback: %s\n", s_config.fallback_path);
    }
    
    /*------------------------------------------------------------------------
     * Phase 6: Optional Boot Delay
     *------------------------------------------------------------------------*/
    
    if (s_config.boot_delay_ms > 0) {
        LOG("Waiting %u ms...\n", s_config.boot_delay_ms);
        hal_delay_ms(s_config.boot_delay_ms);
    }
    
    /*------------------------------------------------------------------------
     * Phase 7: Load ELF Image
     *------------------------------------------------------------------------*/
    
    /* Mark boot attempt */
    mimi_config_boot_attempt(&s_config);
    
    /* Get image path */
    const char* image_path = mimi_config_get_image(&s_config);
    if (image_path == NULL) {
        boot_fail(BLINK_FILE_NOT_FOUND, "No boot image configured");
    }
    
    LOG("Loading: %s\n", image_path);
    
    /* Open ELF file */
    fs_err = fat32_open(&s_fs, image_path, &s_loader_ctx.file);
    if (fs_err != FAT32_OK) {
        /* Try fallback if available */
        if (s_config.has_fallback && s_config.fallback_path[0] != '\0') {
            LOG("Primary image not found, trying fallback...\n");
            image_path = s_config.fallback_path;
            fs_err = fat32_open(&s_fs, image_path, &s_loader_ctx.file);
        }
        
        if (fs_err != FAT32_OK) {
            boot_fail(BLINK_FILE_NOT_FOUND, "Boot image not found");
        }
    }
    
    uint32_t file_size = fat32_size(&s_loader_ctx.file);
    LOG_VERBOSE("File size: %u bytes\n", file_size);
    
    /* Configure loader */
    mimi_mem_region_t ram_region = {
        .base = platform.ram_base,
        .size = platform.ram_size,
        .flags = MIMI_MEM_READ | MIMI_MEM_WRITE | MIMI_MEM_EXEC | MIMI_MEM_RAM,
    };
    
    mimi_loader_config_t loader_config = {
        .regions = &ram_region,
        .region_count = 1,
        .io = &s_loader_io,
        .validate_addresses = true,
        .zero_bss = true,
        .verify_after_load = s_config.verify,
    };
    
    /* Load ELF */
    uint32_t load_start_us = hal_get_time_us();
    
    err = mimi_elf_load(&loader_config, &s_loader_ctx, &load_result);
    
    uint32_t load_time_us = hal_get_time_us() - load_start_us;
    
    if (err != MIMI_OK) {
        LOG("[ERROR] ELF load failed: %s\n", mimi_strerror(err));
        
        /* Try to give more specific error */
        switch (err) {
            case MIMI_ERR_NOT_ELF:
            case MIMI_ERR_NOT_ELF32:
            case MIMI_ERR_NOT_ARM:
            case MIMI_ERR_NOT_EXEC:
                boot_fail(BLINK_ELF_INVALID, mimi_strerror(err));
                break;
            case MIMI_ERR_ADDR_INVALID:
            case MIMI_ERR_TOO_LARGE:
                boot_fail(BLINK_NO_MEMORY, mimi_strerror(err));
                break;
            default:
                boot_fail(BLINK_LOAD_FAIL, mimi_strerror(err));
                break;
        }
    }
    
    /* Validate loaded image */
    err = mimi_elf_validate_loaded(&load_result);
    if (err != MIMI_OK) {
        boot_fail(BLINK_ELF_INVALID, "Image validation failed");
    }
    
    LOG("Loaded successfully!\n");
    LOG_VERBOSE("  Entry point: 0x%08X\n", load_result.entry);
    LOG_VERBOSE("  Load region: 0x%08X - 0x%08X\n", 
        load_result.load_base, load_result.load_end);
    LOG_VERBOSE("  Total size:  %u bytes\n", load_result.total_size);
    LOG_VERBOSE("  Segments:    %u\n", load_result.segment_count);
    LOG_VERBOSE("  Copied:      %u bytes\n", load_result.bytes_copied);
    LOG_VERBOSE("  Zeroed:      %u bytes (BSS)\n", load_result.bytes_zeroed);
    LOG_VERBOSE("  Load time:   %u us\n", load_time_us);
    
    /*------------------------------------------------------------------------
     * Phase 8: Build Handoff Structure
     *------------------------------------------------------------------------*/
    
    LOG_VERBOSE("\nPreparing handoff...\n");
    
    /* Extract filename for handoff */
    const char* filename = image_path;
    const char* p = image_path;
    while (*p) {
        if (*p == '/') filename = p + 1;
        p++;
    }
    
    mimi_handoff_build(&s_handoff, &load_result, &platform, filename);
    
    uint32_t total_boot_time_us = hal_get_time_us() - boot_start_us;
    s_handoff.boot_time_us = total_boot_time_us;
    s_handoff.loader_time_us = load_time_us;
    
    LOG_VERBOSE("Handoff structure at: 0x%08X\n", (uint32_t)&s_handoff);
    LOG_VERBOSE("Total boot time: %u us (%u ms)\n", 
        total_boot_time_us, total_boot_time_us / 1000);
    
    /*------------------------------------------------------------------------
     * Phase 9: Jump to Payload
     *------------------------------------------------------------------------*/
    
    LOG("\n");
    LOG(">>> Jumping to payload at 0x%08X\n", load_result.entry);
    LOG("========================================\n\n");
    
    /* Small delay to let UART flush */
    hal_delay_ms(10);
    
    /* Set LED off before handoff */
    hal_led_set(false);
    
    /* Point of no return */
    mimi_handoff_jump(&s_handoff, load_result.entry);
    
    /* Never reached */
    return 0;
}

/*============================================================================
 * Minimal C Runtime
 *============================================================================*/

/**
 * _start - True entry point from reset vector.
 * 
 * Called by the vector table. Sets up stack and calls main.
 * For RP2040, the Pico SDK handles this. For bare-metal ports,
 * this would be the actual reset handler.
 */

#if !defined(PICO_SDK)
/* Bare-metal startup - defined in startup.S */
#endif
