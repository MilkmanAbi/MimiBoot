/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * fat32.h - FAT32 Filesystem Interface
 * 
 * Minimal read-only FAT32 implementation for bootloader use.
 * Supports:
 * - FAT32 only (not FAT12/FAT16)
 * - Long filenames (LFN) - read-only
 * - File reading
 * - Directory traversal
 */

#ifndef MIMIBOOT_FAT32_H
#define MIMIBOOT_FAT32_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * Constants
 *============================================================================*/

#define FAT32_MAX_PATH      256
#define FAT32_MAX_NAME      256

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    FAT32_OK            = 0,
    FAT32_ERR_IO        = -1,
    FAT32_ERR_NOT_FAT32 = -2,
    FAT32_ERR_NOT_FOUND = -3,
    FAT32_ERR_NOT_FILE  = -4,
    FAT32_ERR_NOT_DIR   = -5,
    FAT32_ERR_EOF       = -6,
    FAT32_ERR_INVALID   = -7,
} fat32_err_t;

/*============================================================================
 * Filesystem Context
 *============================================================================*/

typedef struct {
    /* Partition geometry */
    uint32_t partition_start;       /* First sector of partition */
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sector;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t total_sectors;
    
    /* Computed values */
    uint32_t fat_start;             /* First sector of FAT */
    uint32_t data_start;            /* First sector of data region */
    uint32_t cluster_size;          /* Bytes per cluster */
    
    /* Read callback */
    int (*read_sector)(uint32_t sector, uint8_t* buffer);
    
} fat32_fs_t;

/*============================================================================
 * File Handle
 *============================================================================*/

typedef struct {
    fat32_fs_t* fs;
    uint32_t start_cluster;     /* First cluster of file */
    uint32_t current_cluster;   /* Current cluster being read */
    uint32_t file_size;         /* Total file size */
    uint32_t position;          /* Current read position */
    uint8_t  attr;              /* File attributes */
} fat32_file_t;

/*============================================================================
 * Directory Entry (for iteration)
 *============================================================================*/

typedef struct {
    char        name[FAT32_MAX_NAME];   /* Long or short name */
    uint32_t    size;
    uint32_t    cluster;
    uint8_t     attr;
    bool        is_dir;
} fat32_dirent_t;

/* Attribute flags */
#define FAT32_ATTR_READ_ONLY    0x01
#define FAT32_ATTR_HIDDEN       0x02
#define FAT32_ATTR_SYSTEM       0x04
#define FAT32_ATTR_VOLUME_ID    0x08
#define FAT32_ATTR_DIRECTORY    0x10
#define FAT32_ATTR_ARCHIVE      0x20
#define FAT32_ATTR_LFN          0x0F

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * Mount FAT32 filesystem.
 * 
 * @param fs            Filesystem context to initialize
 * @param read_sector   Callback to read a 512-byte sector
 * @return              FAT32_OK on success, error code otherwise
 */
fat32_err_t fat32_mount(fat32_fs_t* fs, int (*read_sector)(uint32_t, uint8_t*));

/**
 * Open a file by path.
 * 
 * Path uses forward slashes, e.g., "/boot/kernel.elf"
 * 
 * @param fs    Mounted filesystem
 * @param path  File path (absolute)
 * @param file  Output file handle
 * @return      FAT32_OK on success
 */
fat32_err_t fat32_open(fat32_fs_t* fs, const char* path, fat32_file_t* file);

/**
 * Read bytes from an open file.
 * 
 * @param file      Open file handle
 * @param buffer    Destination buffer
 * @param size      Bytes to read
 * @return          Bytes read, or negative error
 */
int32_t fat32_read(fat32_file_t* file, void* buffer, uint32_t size);

/**
 * Seek to position in file.
 * 
 * @param file      Open file handle
 * @param offset    Byte offset from start
 * @return          FAT32_OK on success
 */
fat32_err_t fat32_seek(fat32_file_t* file, uint32_t offset);

/**
 * Get file size.
 * 
 * @param file  Open file handle
 * @return      File size in bytes
 */
uint32_t fat32_size(fat32_file_t* file);

/**
 * Check if path exists.
 * 
 * @param fs    Mounted filesystem
 * @param path  Path to check
 * @return      true if exists
 */
bool fat32_exists(fat32_fs_t* fs, const char* path);

#endif /* MIMIBOOT_FAT32_H */
