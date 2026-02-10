/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * sd_spi.c - SD Card Driver (SPI Mode)
 * 
 * Minimal SD card driver for reading files from FAT32 formatted cards.
 * Supports SD, SDHC, and SDXC cards in SPI mode.
 * 
 * This is a read-only driver - no write support needed for bootloader.
 */

#include "../hal.h"
#include "rp2040_regs.h"
#include <stddef.h>

/*============================================================================
 * SD Card Constants
 *============================================================================*/

/* Commands */
#define CMD0    0       /* GO_IDLE_STATE */
#define CMD1    1       /* SEND_OP_COND (MMC) */
#define CMD8    8       /* SEND_IF_COND */
#define CMD9    9       /* SEND_CSD */
#define CMD10   10      /* SEND_CID */
#define CMD12   12      /* STOP_TRANSMISSION */
#define CMD13   13      /* SEND_STATUS */
#define CMD16   16      /* SET_BLOCKLEN */
#define CMD17   17      /* READ_SINGLE_BLOCK */
#define CMD18   18      /* READ_MULTIPLE_BLOCK */
#define CMD24   24      /* WRITE_BLOCK (not used) */
#define CMD55   55      /* APP_CMD */
#define CMD58   58      /* READ_OCR */
#define ACMD41  41      /* SD_SEND_OP_COND (after CMD55) */

/* R1 Response bits */
#define R1_IDLE_STATE       (1 << 0)
#define R1_ERASE_RESET      (1 << 1)
#define R1_ILLEGAL_CMD      (1 << 2)
#define R1_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQ_ERROR  (1 << 4)
#define R1_ADDRESS_ERROR    (1 << 5)
#define R1_PARAMETER_ERROR  (1 << 6)

/* Data tokens */
#define DATA_TOKEN_CMD17    0xFE
#define DATA_TOKEN_CMD18    0xFE
#define DATA_TOKEN_CMD24    0xFE
#define DATA_TOKEN_CMD25    0xFC

/* Timeouts (in attempts) */
#define SD_INIT_TIMEOUT     1000
#define SD_CMD_TIMEOUT      100
#define SD_READ_TIMEOUT     100000

/*============================================================================
 * Pin Configuration (must match hal_rp2040.c)
 *============================================================================*/

#define SD_CS_PIN           5
#define SD_SPI_INST         0

/*============================================================================
 * Static State
 *============================================================================*/

static struct {
    bool initialized;
    bool sdhc;              /* SDHC/SDXC (block addressing) vs SD (byte addressing) */
    uint32_t block_count;
} s_sd;

/*============================================================================
 * Low-Level SPI Helpers
 *============================================================================*/

/* External reference to SPI state from HAL */
extern int hal_spi_transfer(hal_spi_t spi, const uint8_t* tx, uint8_t* rx, uint32_t len);

/* SPI state reference */
typedef struct {
    uint32_t base;
    uint32_t clock_hz;
} spi_state_t;
extern spi_state_t s_spi_state[2];

static inline void sd_cs_low(void) {
    hal_gpio_write(SD_CS_PIN, false);
}

static inline void sd_cs_high(void) {
    hal_gpio_write(SD_CS_PIN, true);
}

static uint8_t sd_spi_byte(uint8_t out) {
    uint8_t in;
    hal_spi_transfer(&s_spi_state[SD_SPI_INST], &out, &in, 1);
    return in;
}

static void sd_spi_bytes(const uint8_t* tx, uint8_t* rx, uint32_t len) {
    hal_spi_transfer(&s_spi_state[SD_SPI_INST], tx, rx, len);
}

/**
 * Wait for card to be ready (not busy).
 */
static int sd_wait_ready(uint32_t timeout) {
    for (uint32_t i = 0; i < timeout; i++) {
        if (sd_spi_byte(0xFF) == 0xFF) {
            return 0;
        }
    }
    return -1;
}

/*============================================================================
 * Command Interface
 *============================================================================*/

/**
 * Calculate CRC7 for SD commands.
 */
static uint8_t sd_crc7(const uint8_t* data, uint32_t len) {
    uint8_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int bit = 7; bit >= 0; bit--) {
            crc <<= 1;
            if ((byte ^ crc) & 0x80) {
                crc ^= 0x09;  /* x^7 + x^3 + 1 */
            }
            byte <<= 1;
        }
    }
    return (crc << 1) | 1;  /* Shift left, set end bit */
}

/**
 * Send SD command and get R1 response.
 */
static uint8_t sd_command(uint8_t cmd, uint32_t arg) {
    uint8_t frame[6];
    uint8_t resp;
    
    /* Wait for card ready */
    if (sd_wait_ready(SD_CMD_TIMEOUT) != 0) {
        return 0xFF;
    }
    
    /* Build command frame */
    frame[0] = 0x40 | cmd;
    frame[1] = (arg >> 24) & 0xFF;
    frame[2] = (arg >> 16) & 0xFF;
    frame[3] = (arg >> 8) & 0xFF;
    frame[4] = arg & 0xFF;
    frame[5] = sd_crc7(frame, 5);
    
    /* Send command */
    sd_spi_bytes(frame, NULL, 6);
    
    /* Wait for response (R1 format) */
    for (int i = 0; i < SD_CMD_TIMEOUT; i++) {
        resp = sd_spi_byte(0xFF);
        if (!(resp & 0x80)) {
            break;
        }
    }
    
    return resp;
}

/**
 * Send application-specific command (ACMD).
 */
static uint8_t sd_app_command(uint8_t cmd, uint32_t arg) {
    uint8_t resp = sd_command(CMD55, 0);
    if (resp > 1) {
        return resp;
    }
    return sd_command(cmd, arg);
}

/*============================================================================
 * Card Initialization
 *============================================================================*/

int sd_init(void) {
    uint8_t resp;
    uint8_t ocr[4];
    
    s_sd.initialized = false;
    s_sd.sdhc = false;
    s_sd.block_count = 0;
    
    /* CS high, send 80+ clock pulses to wake up card */
    sd_cs_high();
    for (int i = 0; i < 10; i++) {
        sd_spi_byte(0xFF);
    }
    
    /* Select card */
    sd_cs_low();
    
    /* CMD0: GO_IDLE_STATE */
    for (int attempt = 0; attempt < SD_INIT_TIMEOUT; attempt++) {
        resp = sd_command(CMD0, 0);
        if (resp == R1_IDLE_STATE) {
            break;
        }
        if (attempt == SD_INIT_TIMEOUT - 1) {
            sd_cs_high();
            return -1;
        }
    }
    
    /* CMD8: SEND_IF_COND (check for SD v2.0+) */
    resp = sd_command(CMD8, 0x000001AA);
    
    if (resp == R1_IDLE_STATE) {
        /* SD v2.0+ card */
        /* Read R7 response (4 bytes) */
        for (int i = 0; i < 4; i++) {
            ocr[i] = sd_spi_byte(0xFF);
        }
        
        /* Check echo pattern */
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) {
            sd_cs_high();
            return -2;
        }
        
        /* ACMD41: SD_SEND_OP_COND with HCS bit */
        for (int attempt = 0; attempt < SD_INIT_TIMEOUT; attempt++) {
            resp = sd_app_command(ACMD41, 0x40000000);
            if (resp == 0) {
                break;
            }
            if (attempt == SD_INIT_TIMEOUT - 1) {
                sd_cs_high();
                return -3;
            }
            hal_delay_ms(1);
        }
        
        /* CMD58: Read OCR to check CCS bit */
        resp = sd_command(CMD58, 0);
        if (resp != 0) {
            sd_cs_high();
            return -4;
        }
        
        for (int i = 0; i < 4; i++) {
            ocr[i] = sd_spi_byte(0xFF);
        }
        
        /* Check CCS bit (bit 30 of OCR) */
        s_sd.sdhc = (ocr[0] & 0x40) != 0;
        
    } else if (resp == (R1_IDLE_STATE | R1_ILLEGAL_CMD)) {
        /* SD v1.x or MMC card */
        s_sd.sdhc = false;
        
        /* Try ACMD41 first (SD) */
        resp = sd_app_command(ACMD41, 0);
        
        if (resp <= 1) {
            /* SD v1.x card */
            for (int attempt = 0; attempt < SD_INIT_TIMEOUT; attempt++) {
                resp = sd_app_command(ACMD41, 0);
                if (resp == 0) break;
                hal_delay_ms(1);
            }
        } else {
            /* MMC card - use CMD1 */
            for (int attempt = 0; attempt < SD_INIT_TIMEOUT; attempt++) {
                resp = sd_command(CMD1, 0);
                if (resp == 0) break;
                hal_delay_ms(1);
            }
        }
        
        if (resp != 0) {
            sd_cs_high();
            return -5;
        }
        
        /* Set block size to 512 for non-SDHC cards */
        resp = sd_command(CMD16, 512);
        if (resp != 0) {
            sd_cs_high();
            return -6;
        }
        
    } else {
        sd_cs_high();
        return -7;
    }
    
    /* Read CSD to get capacity */
    resp = sd_command(CMD9, 0);
    if (resp == 0) {
        /* Wait for data token */
        for (int i = 0; i < SD_READ_TIMEOUT; i++) {
            resp = sd_spi_byte(0xFF);
            if (resp == DATA_TOKEN_CMD17) break;
        }
        
        if (resp == DATA_TOKEN_CMD17) {
            uint8_t csd[16];
            sd_spi_bytes(NULL, csd, 16);
            
            /* Skip CRC */
            sd_spi_byte(0xFF);
            sd_spi_byte(0xFF);
            
            /* Parse CSD to get block count */
            if ((csd[0] >> 6) == 1) {
                /* CSD v2.0 (SDHC/SDXC) */
                uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                                  ((uint32_t)csd[8] << 8) |
                                  csd[9];
                s_sd.block_count = (c_size + 1) * 1024;
            } else {
                /* CSD v1.0 */
                uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) |
                                  ((uint32_t)csd[7] << 2) |
                                  ((csd[8] >> 6) & 0x03);
                uint32_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
                uint32_t read_bl_len = csd[5] & 0x0F;
                uint32_t mult = 1 << (c_size_mult + 2);
                uint32_t blocknr = (c_size + 1) * mult;
                uint32_t block_len = 1 << read_bl_len;
                s_sd.block_count = blocknr * (block_len / 512);
            }
        }
    }
    
    sd_cs_high();
    s_sd.initialized = true;
    
    return 0;
}

/*============================================================================
 * Block Read
 *============================================================================*/

int sd_read_blocks(uint32_t block, uint8_t* buffer, uint32_t count) {
    if (!s_sd.initialized) {
        return -1;
    }
    
    uint8_t resp;
    uint32_t addr = s_sd.sdhc ? block : (block * 512);
    
    sd_cs_low();
    
    if (count == 1) {
        /* Single block read */
        resp = sd_command(CMD17, addr);
        if (resp != 0) {
            sd_cs_high();
            return -2;
        }
        
        /* Wait for data token */
        for (int i = 0; i < SD_READ_TIMEOUT; i++) {
            resp = sd_spi_byte(0xFF);
            if (resp == DATA_TOKEN_CMD17) break;
            if ((resp & 0xF0) == 0x00) {
                /* Error token */
                sd_cs_high();
                return -3;
            }
        }
        
        if (resp != DATA_TOKEN_CMD17) {
            sd_cs_high();
            return -4;
        }
        
        /* Read data */
        sd_spi_bytes(NULL, buffer, 512);
        
        /* Skip CRC */
        sd_spi_byte(0xFF);
        sd_spi_byte(0xFF);
        
    } else {
        /* Multiple block read */
        resp = sd_command(CMD18, addr);
        if (resp != 0) {
            sd_cs_high();
            return -5;
        }
        
        for (uint32_t b = 0; b < count; b++) {
            /* Wait for data token */
            for (int i = 0; i < SD_READ_TIMEOUT; i++) {
                resp = sd_spi_byte(0xFF);
                if (resp == DATA_TOKEN_CMD18) break;
            }
            
            if (resp != DATA_TOKEN_CMD18) {
                sd_command(CMD12, 0);
                sd_cs_high();
                return -6;
            }
            
            /* Read data */
            sd_spi_bytes(NULL, buffer + (b * 512), 512);
            
            /* Skip CRC */
            sd_spi_byte(0xFF);
            sd_spi_byte(0xFF);
        }
        
        /* Stop transmission */
        sd_command(CMD12, 0);
        sd_spi_byte(0xFF);  /* Extra byte needed */
    }
    
    sd_cs_high();
    return 0;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

uint32_t sd_get_block_count(void) {
    return s_sd.block_count;
}

bool sd_is_initialized(void) {
    return s_sd.initialized;
}

bool sd_is_sdhc(void) {
    return s_sd.sdhc;
}
