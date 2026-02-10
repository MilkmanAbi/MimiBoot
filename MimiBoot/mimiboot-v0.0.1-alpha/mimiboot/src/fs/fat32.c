/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * fat32.c - FAT32 Filesystem Implementation
 * 
 * Minimal read-only FAT32 driver. Supports FAT32 only (not FAT12/FAT16).
 */

#include "fat32.h"
#include <stddef.h>

/*============================================================================
 * Internal Constants
 *============================================================================*/

/* Boot sector offsets */
#define BPB_BYTES_PER_SEC       11
#define BPB_SEC_PER_CLUS        13
#define BPB_RSVD_SEC_CNT        14
#define BPB_NUM_FATS            16
#define BPB_TOT_SEC_16          19
#define BPB_TOT_SEC_32          32
#define BPB_FAT_SZ_32           36
#define BPB_ROOT_CLUS           44
#define BPB_FS_TYPE             82

/* Directory entry offsets */
#define DIR_NAME                0
#define DIR_ATTR                11
#define DIR_NTRes               12
#define DIR_CRT_TIME_TENTH      13
#define DIR_CRT_TIME            14
#define DIR_CRT_DATE            16
#define DIR_LST_ACC_DATE        18
#define DIR_FST_CLUS_HI         20
#define DIR_WRT_TIME            22
#define DIR_WRT_DATE            24
#define DIR_FST_CLUS_LO         26
#define DIR_FILE_SIZE           28

/* LFN entry offsets */
#define LFN_ORD                 0
#define LFN_NAME1               1
#define LFN_ATTR                11
#define LFN_TYPE                12
#define LFN_CHKSUM              13
#define LFN_NAME2               14
#define LFN_NAME3               28

#define LFN_LAST_ENTRY          0x40

/* FAT entry values */
#define FAT32_EOC               0x0FFFFFF8
#define FAT32_BAD               0x0FFFFFF7

/*============================================================================
 * Internal Helpers - No libc dependency
 *============================================================================*/

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a++ != *b++) return false;
    }
    return *a == *b;
}

static int str_len(const char* s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

static void mem_zero(void* p, uint32_t size) {
    uint8_t* d = (uint8_t*)p;
    while (size--) *d++ = 0;
}

/*============================================================================
 * Cluster Operations
 *============================================================================*/

/**
 * Convert cluster number to first sector of that cluster.
 */
static uint32_t cluster_to_sector(fat32_fs_t* fs, uint32_t cluster) {
    return fs->data_start + (cluster - 2) * fs->sectors_per_cluster;
}

/**
 * Read next cluster number from FAT.
 */
static uint32_t fat_next_cluster(fat32_fs_t* fs, uint32_t cluster) {
    uint8_t buffer[512];
    
    /* Calculate FAT sector and offset */
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    if (fs->read_sector(fat_sector, buffer) != 0) {
        return FAT32_EOC;
    }
    
    uint32_t next = read_u32(&buffer[entry_offset]) & 0x0FFFFFFF;
    return next;
}

/**
 * Check if cluster number indicates end of chain.
 */
static bool is_eoc(uint32_t cluster) {
    return cluster >= FAT32_EOC || cluster < 2;
}

/*============================================================================
 * Mount
 *============================================================================*/

fat32_err_t fat32_mount(fat32_fs_t* fs, int (*read_sector)(uint32_t, uint8_t*)) {
    uint8_t buffer[512];
    
    mem_zero(fs, sizeof(fat32_fs_t));
    fs->read_sector = read_sector;
    
    /* Read MBR to find partition */
    if (read_sector(0, buffer) != 0) {
        return FAT32_ERR_IO;
    }
    
    /* Check for MBR signature */
    if (buffer[510] != 0x55 || buffer[511] != 0xAA) {
        return FAT32_ERR_NOT_FAT32;
    }
    
    /* Check partition table entry 0 */
    /* Partition entry at offset 446, type at +4, LBA start at +8 */
    uint8_t part_type = buffer[446 + 4];
    uint32_t part_start = read_u32(&buffer[446 + 8]);
    
    if (part_type == 0x0B || part_type == 0x0C) {
        /* FAT32 partition */
        fs->partition_start = part_start;
    } else if (buffer[0] == 0xEB || buffer[0] == 0xE9) {
        /* No MBR, boot sector is at sector 0 (superfloppy) */
        fs->partition_start = 0;
    } else {
        return FAT32_ERR_NOT_FAT32;
    }
    
    /* Read boot sector */
    if (read_sector(fs->partition_start, buffer) != 0) {
        return FAT32_ERR_IO;
    }
    
    /* Validate boot sector */
    if (buffer[510] != 0x55 || buffer[511] != 0xAA) {
        return FAT32_ERR_NOT_FAT32;
    }
    
    /* Parse BPB */
    fs->bytes_per_sector = read_u16(&buffer[BPB_BYTES_PER_SEC]);
    fs->sectors_per_cluster = buffer[BPB_SEC_PER_CLUS];
    fs->reserved_sectors = read_u16(&buffer[BPB_RSVD_SEC_CNT]);
    fs->fat_count = buffer[BPB_NUM_FATS];
    fs->sectors_per_fat = read_u32(&buffer[BPB_FAT_SZ_32]);
    fs->root_cluster = read_u32(&buffer[BPB_ROOT_CLUS]);
    
    uint32_t tot_sec_16 = read_u16(&buffer[BPB_TOT_SEC_16]);
    fs->total_sectors = (tot_sec_16 != 0) ? tot_sec_16 : read_u32(&buffer[BPB_TOT_SEC_32]);
    
    /* Validate FAT32 */
    if (fs->bytes_per_sector != 512) {
        return FAT32_ERR_NOT_FAT32;
    }
    
    /* Calculate derived values */
    fs->fat_start = fs->partition_start + fs->reserved_sectors;
    fs->data_start = fs->fat_start + (fs->fat_count * fs->sectors_per_fat);
    fs->cluster_size = fs->sectors_per_cluster * fs->bytes_per_sector;
    
    return FAT32_OK;
}

/*============================================================================
 * Directory Operations
 *============================================================================*/

/**
 * Parse 8.3 short name from directory entry.
 */
static void parse_short_name(const uint8_t* entry, char* name) {
    int i, j = 0;
    
    /* Name (8 chars) */
    for (i = 0; i < 8 && entry[i] != ' '; i++) {
        name[j++] = entry[i];
    }
    
    /* Extension (3 chars) */
    if (entry[8] != ' ') {
        name[j++] = '.';
        for (i = 8; i < 11 && entry[i] != ' '; i++) {
            name[j++] = entry[i];
        }
    }
    
    name[j] = '\0';
}

/**
 * Compare filename against pattern (case-insensitive).
 */
static bool name_match(const char* name, const char* pattern) {
    while (*name && *pattern) {
        if (to_upper(*name) != to_upper(*pattern)) {
            return false;
        }
        name++;
        pattern++;
    }
    return *name == *pattern;
}

/**
 * Search directory for entry by name.
 * Returns cluster of found entry, or 0 if not found.
 */
static fat32_err_t find_in_dir(
    fat32_fs_t* fs,
    uint32_t dir_cluster,
    const char* name,
    fat32_dirent_t* out
) {
    uint8_t buffer[512];
    char entry_name[FAT32_MAX_NAME];
    char lfn_buffer[FAT32_MAX_NAME];
    int lfn_index = 0;
    bool lfn_valid = false;
    
    mem_zero(lfn_buffer, sizeof(lfn_buffer));
    
    uint32_t cluster = dir_cluster;
    
    while (!is_eoc(cluster)) {
        uint32_t sector = cluster_to_sector(fs, cluster);
        
        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            if (fs->read_sector(sector + s, buffer) != 0) {
                return FAT32_ERR_IO;
            }
            
            for (int e = 0; e < 16; e++) {
                uint8_t* entry = &buffer[e * 32];
                
                /* Check for end of directory */
                if (entry[0] == 0x00) {
                    return FAT32_ERR_NOT_FOUND;
                }
                
                /* Skip deleted entries */
                if (entry[0] == 0xE5) {
                    lfn_valid = false;
                    continue;
                }
                
                uint8_t attr = entry[DIR_ATTR];
                
                /* Handle LFN entry */
                if (attr == FAT32_ATTR_LFN) {
                    uint8_t ord = entry[LFN_ORD];
                    
                    if (ord & LFN_LAST_ENTRY) {
                        /* Start of LFN sequence */
                        mem_zero(lfn_buffer, sizeof(lfn_buffer));
                        lfn_valid = true;
                    }
                    
                    if (lfn_valid) {
                        int seq = (ord & 0x1F) - 1;
                        int base = seq * 13;
                        
                        /* Extract UCS-2 characters (simplified - ASCII only) */
                        lfn_buffer[base + 0] = entry[1];
                        lfn_buffer[base + 1] = entry[3];
                        lfn_buffer[base + 2] = entry[5];
                        lfn_buffer[base + 3] = entry[7];
                        lfn_buffer[base + 4] = entry[9];
                        lfn_buffer[base + 5] = entry[14];
                        lfn_buffer[base + 6] = entry[16];
                        lfn_buffer[base + 7] = entry[18];
                        lfn_buffer[base + 8] = entry[20];
                        lfn_buffer[base + 9] = entry[22];
                        lfn_buffer[base + 10] = entry[24];
                        lfn_buffer[base + 11] = entry[28];
                        lfn_buffer[base + 12] = entry[30];
                    }
                    continue;
                }
                
                /* Skip volume label */
                if (attr & FAT32_ATTR_VOLUME_ID) {
                    lfn_valid = false;
                    continue;
                }
                
                /* Regular entry - get name */
                if (lfn_valid && lfn_buffer[0] != '\0') {
                    /* Use LFN */
                    for (int i = 0; i < FAT32_MAX_NAME - 1 && lfn_buffer[i]; i++) {
                        entry_name[i] = lfn_buffer[i];
                        entry_name[i + 1] = '\0';
                    }
                } else {
                    /* Use short name */
                    parse_short_name(entry, entry_name);
                }
                
                lfn_valid = false;
                
                /* Check for match */
                if (name_match(entry_name, name)) {
                    out->size = read_u32(&entry[DIR_FILE_SIZE]);
                    out->cluster = ((uint32_t)read_u16(&entry[DIR_FST_CLUS_HI]) << 16) |
                                   read_u16(&entry[DIR_FST_CLUS_LO]);
                    out->attr = attr;
                    out->is_dir = (attr & FAT32_ATTR_DIRECTORY) != 0;
                    
                    int len = str_len(entry_name);
                    for (int i = 0; i <= len; i++) {
                        out->name[i] = entry_name[i];
                    }
                    
                    return FAT32_OK;
                }
            }
        }
        
        cluster = fat_next_cluster(fs, cluster);
    }
    
    return FAT32_ERR_NOT_FOUND;
}

/*============================================================================
 * File Operations
 *============================================================================*/

fat32_err_t fat32_open(fat32_fs_t* fs, const char* path, fat32_file_t* file) {
    fat32_dirent_t dirent;
    uint32_t current_cluster = fs->root_cluster;
    
    mem_zero(file, sizeof(fat32_file_t));
    
    /* Skip leading slash */
    if (*path == '/') path++;
    
    /* Handle empty path (root directory) */
    if (*path == '\0') {
        file->fs = fs;
        file->start_cluster = fs->root_cluster;
        file->current_cluster = fs->root_cluster;
        file->file_size = 0;
        file->position = 0;
        file->attr = FAT32_ATTR_DIRECTORY;
        return FAT32_OK;
    }
    
    /* Parse path components */
    while (*path) {
        /* Extract component name */
        char component[FAT32_MAX_NAME];
        int i = 0;
        
        while (*path && *path != '/' && i < FAT32_MAX_NAME - 1) {
            component[i++] = *path++;
        }
        component[i] = '\0';
        
        /* Skip slash */
        if (*path == '/') path++;
        
        /* Find component in current directory */
        fat32_err_t err = find_in_dir(fs, current_cluster, component, &dirent);
        if (err != FAT32_OK) {
            return err;
        }
        
        /* If more path remains, this must be a directory */
        if (*path && !dirent.is_dir) {
            return FAT32_ERR_NOT_DIR;
        }
        
        current_cluster = dirent.cluster;
    }
    
    /* Fill in file handle */
    file->fs = fs;
    file->start_cluster = dirent.cluster;
    file->current_cluster = dirent.cluster;
    file->file_size = dirent.size;
    file->position = 0;
    file->attr = dirent.attr;
    
    return FAT32_OK;
}

int32_t fat32_read(fat32_file_t* file, void* buffer, uint32_t size) {
    uint8_t sector_buf[512];
    uint8_t* out = (uint8_t*)buffer;
    uint32_t bytes_read = 0;
    fat32_fs_t* fs = file->fs;
    
    /* Limit read to remaining file size */
    if (file->position + size > file->file_size) {
        size = file->file_size - file->position;
    }
    
    if (size == 0) {
        return 0;
    }
    
    while (bytes_read < size) {
        /* Check for end of cluster chain */
        if (is_eoc(file->current_cluster)) {
            break;
        }
        
        /* Calculate position within cluster */
        uint32_t cluster_offset = file->position % fs->cluster_size;
        uint32_t sector_in_cluster = cluster_offset / 512;
        uint32_t offset_in_sector = cluster_offset % 512;
        
        /* Read sector */
        uint32_t sector = cluster_to_sector(fs, file->current_cluster) + sector_in_cluster;
        if (fs->read_sector(sector, sector_buf) != 0) {
            return (bytes_read > 0) ? bytes_read : FAT32_ERR_IO;
        }
        
        /* Copy data from sector */
        uint32_t copy_len = 512 - offset_in_sector;
        if (copy_len > (size - bytes_read)) {
            copy_len = size - bytes_read;
        }
        
        for (uint32_t i = 0; i < copy_len; i++) {
            out[bytes_read + i] = sector_buf[offset_in_sector + i];
        }
        
        bytes_read += copy_len;
        file->position += copy_len;
        
        /* Move to next cluster if needed */
        if ((file->position % fs->cluster_size) == 0) {
            file->current_cluster = fat_next_cluster(fs, file->current_cluster);
        }
    }
    
    return bytes_read;
}

fat32_err_t fat32_seek(fat32_file_t* file, uint32_t offset) {
    fat32_fs_t* fs = file->fs;
    
    if (offset > file->file_size) {
        offset = file->file_size;
    }
    
    /* Determine target cluster */
    uint32_t target_cluster_index = offset / fs->cluster_size;
    
    /* Walk cluster chain from start */
    uint32_t cluster = file->start_cluster;
    for (uint32_t i = 0; i < target_cluster_index && !is_eoc(cluster); i++) {
        cluster = fat_next_cluster(fs, cluster);
    }
    
    file->current_cluster = cluster;
    file->position = offset;
    
    return FAT32_OK;
}

uint32_t fat32_size(fat32_file_t* file) {
    return file->file_size;
}

bool fat32_exists(fat32_fs_t* fs, const char* path) {
    fat32_file_t file;
    return fat32_open(fs, path, &file) == FAT32_OK;
}
