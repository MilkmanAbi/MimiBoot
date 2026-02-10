/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * hal.h - Hardware Abstraction Layer Interface
 * 
 * This header defines the interface that each platform must implement.
 * The core bootloader code is platform-agnostic and relies on these
 * functions for hardware interaction.
 * 
 * To port MimiBoot to a new platform:
 * 1. Create a new directory under src/hal/
 * 2. Implement all functions declared here
 * 3. Define platform-specific constants
 * 4. Add to the build system
 */

#ifndef MIMIBOOT_HAL_H
#define MIMIBOOT_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*============================================================================
 * Platform Information
 *============================================================================*/

/**
 * Platform information structure.
 * Filled by hal_get_platform_info().
 */
typedef struct {
    /* Memory layout */
    uint32_t    ram_base;       /* RAM base address */
    uint32_t    ram_size;       /* RAM size in bytes */
    uint32_t    loader_base;    /* MimiBoot flash location */
    uint32_t    loader_size;    /* MimiBoot flash size */
    
    /* System state */
    uint32_t    sys_clock_hz;   /* Current system clock frequency */
    uint32_t    reset_reason;   /* Reset cause (MIMI_BOOT_* flags) */
    uint32_t    boot_source;    /* Storage type (MIMI_SOURCE_* flags) */
    
    /* Platform identification */
    uint32_t    chip_id;        /* Chip identification (if available) */
    const char* platform_name;  /* Human-readable name */
    
} mimi_platform_info_t;

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * Early platform initialization.
 * 
 * Called immediately after reset. Should perform minimal initialization:
 * - Clock setup (or verify ROM left clocks in usable state)
 * - Watchdog disable or feed (if needed)
 * - Any critical early setup
 * 
 * Console/storage are initialized separately.
 * 
 * @return  0 on success, negative on error
 */
int hal_init_early(void);

/**
 * Get platform information.
 * 
 * @param info  Output platform info structure
 */
void hal_get_platform_info(mimi_platform_info_t* info);

/*============================================================================
 * Console (Debug Output)
 *============================================================================*/

/**
 * Initialize debug console.
 * 
 * Typically UART at a fixed baud rate. Optional - can be no-op
 * if debug output is not needed.
 * 
 * @return  0 on success, negative on error
 */
int hal_console_init(void);

/**
 * Write a character to console.
 * 
 * @param c     Character to write
 */
void hal_console_putc(char c);

/**
 * Write a string to console.
 * 
 * @param s     Null-terminated string
 */
void hal_console_puts(const char* s);

/**
 * Write formatted output to console (minimal printf).
 * 
 * Supported format specifiers:
 * - %d, %i: signed decimal
 * - %u: unsigned decimal
 * - %x, %X: hexadecimal
 * - %s: string
 * - %c: character
 * - %%: literal %
 * 
 * @param fmt   Format string
 * @param ...   Arguments
 */
void hal_console_printf(const char* fmt, ...);

/*============================================================================
 * Timing
 *============================================================================*/

/**
 * Get time since boot in microseconds.
 * 
 * Doesn't need to be precise, used for timing measurements
 * and boot time reporting.
 * 
 * @return  Microseconds since boot (may wrap)
 */
uint32_t hal_get_time_us(void);

/**
 * Delay for specified microseconds.
 * 
 * @param us    Microseconds to delay
 */
void hal_delay_us(uint32_t us);

/**
 * Delay for specified milliseconds.
 * 
 * @param ms    Milliseconds to delay
 */
void hal_delay_ms(uint32_t ms);

/*============================================================================
 * Storage Interface
 *============================================================================*/

/**
 * Storage device handle.
 */
typedef void* hal_storage_t;

/**
 * Storage device information.
 */
typedef struct {
    uint32_t    sector_size;    /* Sector size in bytes */
    uint32_t    sector_count;   /* Total number of sectors */
    uint32_t    total_size;     /* Total size in bytes */
    bool        readonly;       /* Read-only device */
    const char* name;           /* Device name */
} hal_storage_info_t;

/**
 * Initialize storage subsystem.
 * 
 * Called before any storage access. Sets up SPI, GPIO, etc.
 * 
 * @return  0 on success, negative on error
 */
int hal_storage_init(void);

/**
 * Probe and initialize storage device.
 * 
 * @param dev   Output: storage device handle
 * @return      0 on success, negative on error
 */
int hal_storage_open(hal_storage_t* dev);

/**
 * Close storage device.
 * 
 * @param dev   Storage device handle
 */
void hal_storage_close(hal_storage_t dev);

/**
 * Get storage device information.
 * 
 * @param dev   Storage device handle
 * @param info  Output: device information
 * @return      0 on success, negative on error
 */
int hal_storage_info(hal_storage_t dev, hal_storage_info_t* info);

/**
 * Read bytes from storage.
 * 
 * @param dev       Storage device handle
 * @param offset    Byte offset from start of device
 * @param buffer    Destination buffer
 * @param size      Number of bytes to read
 * @return          Number of bytes read, or negative on error
 */
int32_t hal_storage_read(hal_storage_t dev, uint32_t offset, void* buffer, uint32_t size);

/*============================================================================
 * GPIO (minimal interface for storage)
 *============================================================================*/

/**
 * GPIO pin mode.
 */
typedef enum {
    HAL_GPIO_INPUT,
    HAL_GPIO_OUTPUT,
    HAL_GPIO_INPUT_PULLUP,
    HAL_GPIO_INPUT_PULLDOWN,
    HAL_GPIO_ALT_FUNC,
} hal_gpio_mode_t;

/**
 * Configure GPIO pin mode.
 * 
 * @param pin   Pin number (platform-specific)
 * @param mode  Pin mode
 */
void hal_gpio_set_mode(uint32_t pin, hal_gpio_mode_t mode);

/**
 * Set GPIO output state.
 * 
 * @param pin   Pin number
 * @param state true = high, false = low
 */
void hal_gpio_write(uint32_t pin, bool state);

/**
 * Read GPIO input state.
 * 
 * @param pin   Pin number
 * @return      true if high, false if low
 */
bool hal_gpio_read(uint32_t pin);

/*============================================================================
 * SPI (for SD card / SPI flash)
 *============================================================================*/

/**
 * SPI instance handle.
 */
typedef void* hal_spi_t;

/**
 * SPI configuration.
 */
typedef struct {
    uint32_t    clock_hz;       /* Clock frequency */
    uint8_t     mode;           /* SPI mode (0-3) */
    bool        msb_first;      /* MSB first if true */
} hal_spi_config_t;

/**
 * Initialize SPI peripheral.
 * 
 * @param spi       Output: SPI handle
 * @param instance  SPI peripheral instance (platform-specific, e.g., 0 or 1)
 * @param config    Configuration
 * @return          0 on success, negative on error
 */
int hal_spi_init(hal_spi_t* spi, uint32_t instance, const hal_spi_config_t* config);

/**
 * Transfer bytes over SPI (full duplex).
 * 
 * @param spi       SPI handle
 * @param tx        Transmit buffer (can be NULL for receive-only)
 * @param rx        Receive buffer (can be NULL for transmit-only)
 * @param len       Number of bytes to transfer
 * @return          0 on success, negative on error
 */
int hal_spi_transfer(hal_spi_t spi, const uint8_t* tx, uint8_t* rx, uint32_t len);

/**
 * Set SPI clock frequency.
 * 
 * @param spi       SPI handle
 * @param clock_hz  New clock frequency
 * @return          Actual clock frequency achieved
 */
uint32_t hal_spi_set_clock(hal_spi_t spi, uint32_t clock_hz);

/*============================================================================
 * System Control
 *============================================================================*/

/**
 * System reset.
 * 
 * Performs a software reset of the entire system.
 * This function should not return.
 */
__attribute__((noreturn))
void hal_system_reset(void);

/**
 * Enter low-power halt state.
 * 
 * Used when boot fails and there's nothing else to do.
 * May blink an LED or similar to indicate failure.
 * This function should not return.
 */
__attribute__((noreturn))
void hal_system_halt(void);

/*============================================================================
 * LED Indicator (optional)
 *============================================================================*/

/**
 * Set status LED state.
 * 
 * Used for boot progress indication. Optional - can be no-op.
 * 
 * @param on    true to turn LED on
 */
void hal_led_set(bool on);

/**
 * Blink LED pattern.
 * 
 * Used for error indication.
 * 
 * @param count     Number of blinks
 * @param on_ms     On time in milliseconds
 * @param off_ms    Off time in milliseconds
 */
void hal_led_blink(uint32_t count, uint32_t on_ms, uint32_t off_ms);

#endif /* MIMIBOOT_HAL_H */
