/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * hal_rp2040.c - RP2040/RP2350 Hardware Abstraction Layer
 * 
 * This file implements the HAL interface for Raspberry Pi Pico
 * and Pico 2 (RP2040/RP2350) microcontrollers.
 * 
 * While we use Pico SDK for compilation convenience, this implementation
 * accesses hardware registers directly to keep the code portable and
 * understandable. The same patterns can be applied to bare-metal builds.
 * 
 * Memory Map (RP2040):
 * - Flash: 0x10000000, typically 2MB (external QSPI)
 * - SRAM:  0x20000000, 264KB (striped across 6 banks)
 * - Peripherals: 0x40000000+
 */

#include "../hal.h"
#include "rp2040_regs.h"
#include <stdarg.h>

/*============================================================================
 * Platform Constants
 *============================================================================*/

/* Default clock after boot ROM - 125MHz on RP2040 */
#define XOSC_MHZ            12
#define SYS_CLK_HZ          125000000

/* UART for console */
#define CONSOLE_UART        UART0_BASE
#define CONSOLE_BAUD        115200
#define CONSOLE_TX_PIN      0
#define CONSOLE_RX_PIN      1

/* SD Card SPI pins (directly defined for now, make configurable later) */
#define SD_SPI_INST         0           /* SPI0 */
#define SD_CS_PIN           5
#define SD_SCK_PIN          2
#define SD_MOSI_PIN         3
#define SD_MISO_PIN         4

/* LED pin (Pico onboard) */
#define LED_PIN             25

/* Memory layout */
#define FLASH_BASE          0x10000000
#define SRAM_BASE           0x20000000

#if defined(TARGET_RP2350)
#define SRAM_SIZE           (520 * 1024)    /* 520KB on RP2350 */
#else
#define SRAM_SIZE           (264 * 1024)    /* 264KB on RP2040 */
#endif

/* MimiBoot occupies first 16KB of flash (after boot2) */
#define LOADER_OFFSET       0x100           /* After boot2 */
#define LOADER_SIZE         (16 * 1024)

/*============================================================================
 * Static State
 *============================================================================*/

static uint32_t s_sys_clock_hz = SYS_CLK_HZ;
static bool s_console_initialized = false;
static bool s_timer_initialized = false;

/*============================================================================
 * Register Access Helpers
 *============================================================================*/

static inline void reg_write(uint32_t addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
}

static inline uint32_t reg_read(uint32_t addr) {
    return *(volatile uint32_t*)addr;
}

static inline void reg_set_bits(uint32_t addr, uint32_t bits) {
    *(volatile uint32_t*)(addr + REG_ALIAS_SET_BITS) = bits;
}

static inline void reg_clear_bits(uint32_t addr, uint32_t bits) {
    *(volatile uint32_t*)(addr + REG_ALIAS_CLR_BITS) = bits;
}

/*============================================================================
 * Initialization
 *============================================================================*/

int hal_init_early(void) {
    /*
     * RP2040 boot ROM leaves the system in a usable state:
     * - XOSC running
     * - PLLs configured for 125MHz
     * - Clocks distributed
     * 
     * We just need to ensure peripherals we use are reset
     * and out of reset state.
     */
    
    /* Reset GPIO, UART, SPI, Timer - then release */
    uint32_t reset_mask = 
        (1 << RESET_IO_BANK0) |
        (1 << RESET_PADS_BANK0) |
        (1 << RESET_UART0) |
        (1 << RESET_SPI0) |
        (1 << RESET_TIMER);
    
    /* Assert reset */
    reg_set_bits(RESETS_BASE + RESETS_RESET_OFFSET, reset_mask);
    
    /* Deassert reset */
    reg_clear_bits(RESETS_BASE + RESETS_RESET_OFFSET, reset_mask);
    
    /* Wait for reset done */
    while ((reg_read(RESETS_BASE + RESETS_RESET_DONE_OFFSET) & reset_mask) != reset_mask) {
        /* spin */
    }
    
    /* Initialize timer for timing functions */
    s_timer_initialized = true;
    
    return 0;
}

void hal_get_platform_info(mimi_platform_info_t* info) {
    info->ram_base = SRAM_BASE;
    info->ram_size = SRAM_SIZE;
    info->loader_base = FLASH_BASE + LOADER_OFFSET;
    info->loader_size = LOADER_SIZE;
    info->sys_clock_hz = s_sys_clock_hz;
    
    /* Read reset reason from watchdog scratch or VREG */
    /* For now, assume cold boot */
    info->reset_reason = MIMI_BOOT_COLD;
    
    /* We'll set boot source when storage is mounted */
    info->boot_source = MIMI_SOURCE_SD;
    
    /* Read chip ID from ROM */
#if defined(TARGET_RP2350)
    info->chip_id = 0x2350;
    info->platform_name = "RP2350";
#else
    info->chip_id = 0x2040;
    info->platform_name = "RP2040";
#endif
}

/*============================================================================
 * Console (UART)
 *============================================================================*/

int hal_console_init(void) {
    if (s_console_initialized) {
        return 0;
    }
    
    /* Configure TX pin for UART function */
    /* GPIO function select: UART = 2 */
    reg_write(IO_BANK0_BASE + IO_BANK0_GPIO_CTRL(CONSOLE_TX_PIN), 2);
    reg_write(IO_BANK0_BASE + IO_BANK0_GPIO_CTRL(CONSOLE_RX_PIN), 2);
    
    /* Configure UART */
    /* Baud rate divisor: UARTIBRD and UARTFBRD */
    /* Baud = UARTCLK / (16 * (IBRD + FBRD/64)) */
    /* UARTCLK = sys_clk typically */
    uint32_t baud_div = (s_sys_clock_hz * 4) / CONSOLE_BAUD;
    uint32_t ibrd = baud_div >> 6;
    uint32_t fbrd = baud_div & 0x3F;
    
    reg_write(CONSOLE_UART + UART_IBRD_OFFSET, ibrd);
    reg_write(CONSOLE_UART + UART_FBRD_OFFSET, fbrd);
    
    /* 8N1, FIFO enabled */
    reg_write(CONSOLE_UART + UART_LCR_H_OFFSET, 
        UART_LCR_H_WLEN_8 | UART_LCR_H_FEN);
    
    /* Enable UART, TX, RX */
    reg_write(CONSOLE_UART + UART_CR_OFFSET,
        UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE);
    
    s_console_initialized = true;
    return 0;
}

void hal_console_putc(char c) {
    if (!s_console_initialized) return;
    
    /* Wait for TX FIFO not full */
    while (reg_read(CONSOLE_UART + UART_FR_OFFSET) & UART_FR_TXFF) {
        /* spin */
    }
    
    reg_write(CONSOLE_UART + UART_DR_OFFSET, c);
}

void hal_console_puts(const char* s) {
    while (*s) {
        if (*s == '\n') {
            hal_console_putc('\r');
        }
        hal_console_putc(*s++);
    }
}

/* Minimal printf implementation */
static void print_uint(uint32_t val, int base, int uppercase) {
    char buf[12];
    int i = 0;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    
    if (val == 0) {
        hal_console_putc('0');
        return;
    }
    
    while (val > 0) {
        buf[i++] = digits[val % base];
        val /= base;
    }
    
    while (i > 0) {
        hal_console_putc(buf[--i]);
    }
}

static void print_int(int32_t val) {
    if (val < 0) {
        hal_console_putc('-');
        val = -val;
    }
    print_uint((uint32_t)val, 10, 0);
}

void hal_console_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt != '%') {
            if (*fmt == '\n') hal_console_putc('\r');
            hal_console_putc(*fmt++);
            continue;
        }
        
        fmt++;
        switch (*fmt) {
            case 'd':
            case 'i':
                print_int(va_arg(args, int32_t));
                break;
            case 'u':
                print_uint(va_arg(args, uint32_t), 10, 0);
                break;
            case 'x':
                print_uint(va_arg(args, uint32_t), 16, 0);
                break;
            case 'X':
                print_uint(va_arg(args, uint32_t), 16, 1);
                break;
            case 's':
                hal_console_puts(va_arg(args, const char*));
                break;
            case 'c':
                hal_console_putc((char)va_arg(args, int));
                break;
            case '%':
                hal_console_putc('%');
                break;
            default:
                hal_console_putc('%');
                hal_console_putc(*fmt);
                break;
        }
        fmt++;
    }
    
    va_end(args);
}

/*============================================================================
 * Timing
 *============================================================================*/

uint32_t hal_get_time_us(void) {
    /* RP2040 timer runs at 1MHz */
    return reg_read(TIMER_BASE + TIMER_TIMELR_OFFSET);
}

void hal_delay_us(uint32_t us) {
    uint32_t start = hal_get_time_us();
    while ((hal_get_time_us() - start) < us) {
        /* spin */
    }
}

void hal_delay_ms(uint32_t ms) {
    hal_delay_us(ms * 1000);
}

/*============================================================================
 * GPIO
 *============================================================================*/

void hal_gpio_set_mode(uint32_t pin, hal_gpio_mode_t mode) {
    /* Configure pad */
    uint32_t pad_ctrl = PADS_BANK0_GPIO_OD_DISABLE;  /* No output disable */
    
    switch (mode) {
        case HAL_GPIO_INPUT:
            pad_ctrl |= PADS_BANK0_GPIO_IE;
            break;
        case HAL_GPIO_INPUT_PULLUP:
            pad_ctrl |= PADS_BANK0_GPIO_IE | PADS_BANK0_GPIO_PUE;
            break;
        case HAL_GPIO_INPUT_PULLDOWN:
            pad_ctrl |= PADS_BANK0_GPIO_IE | PADS_BANK0_GPIO_PDE;
            break;
        case HAL_GPIO_OUTPUT:
            pad_ctrl |= PADS_BANK0_GPIO_IE;  /* IE still needed for readback */
            break;
        case HAL_GPIO_ALT_FUNC:
            pad_ctrl |= PADS_BANK0_GPIO_IE;
            break;
    }
    
    reg_write(PADS_BANK0_BASE + PADS_BANK0_GPIO_OFFSET(pin), pad_ctrl);
    
    /* Configure function */
    if (mode == HAL_GPIO_OUTPUT || mode == HAL_GPIO_INPUT || 
        mode == HAL_GPIO_INPUT_PULLUP || mode == HAL_GPIO_INPUT_PULLDOWN) {
        /* SIO function = 5 */
        reg_write(IO_BANK0_BASE + IO_BANK0_GPIO_CTRL(pin), 5);
        
        if (mode == HAL_GPIO_OUTPUT) {
            /* Enable output */
            reg_set_bits(SIO_BASE + SIO_GPIO_OE_OFFSET, (1 << pin));
        } else {
            /* Disable output */
            reg_clear_bits(SIO_BASE + SIO_GPIO_OE_OFFSET, (1 << pin));
        }
    }
}

void hal_gpio_write(uint32_t pin, bool state) {
    if (state) {
        reg_write(SIO_BASE + SIO_GPIO_OUT_SET_OFFSET, (1 << pin));
    } else {
        reg_write(SIO_BASE + SIO_GPIO_OUT_CLR_OFFSET, (1 << pin));
    }
}

bool hal_gpio_read(uint32_t pin) {
    return (reg_read(SIO_BASE + SIO_GPIO_IN_OFFSET) & (1 << pin)) != 0;
}

/*============================================================================
 * SPI
 *============================================================================*/

/* SPI state */
typedef struct {
    uint32_t base;
    uint32_t clock_hz;
} spi_state_t;

static spi_state_t s_spi_state[2];

int hal_spi_init(hal_spi_t* spi, uint32_t instance, const hal_spi_config_t* config) {
    if (instance > 1) return -1;
    
    uint32_t base = (instance == 0) ? SPI0_BASE : SPI1_BASE;
    spi_state_t* state = &s_spi_state[instance];
    state->base = base;
    
    /* Reset SPI */
    uint32_t reset_bit = (instance == 0) ? (1 << RESET_SPI0) : (1 << RESET_SPI1);
    reg_set_bits(RESETS_BASE + RESETS_RESET_OFFSET, reset_bit);
    reg_clear_bits(RESETS_BASE + RESETS_RESET_OFFSET, reset_bit);
    while (!(reg_read(RESETS_BASE + RESETS_RESET_DONE_OFFSET) & reset_bit)) {}
    
    /* Configure clock */
    hal_spi_set_clock(state, config->clock_hz);
    
    /* Configure format: 8 bits, SPI mode, Motorola format */
    uint32_t cr0 = (7 << 0);  /* 8 bit data */
    
    /* SPI mode (CPOL, CPHA) */
    if (config->mode & 1) cr0 |= SPI_SSPCR0_SPH;  /* CPHA */
    if (config->mode & 2) cr0 |= SPI_SSPCR0_SPO;  /* CPOL */
    
    reg_write(base + SPI_SSPCR0_OFFSET, cr0);
    
    /* Enable SPI */
    reg_write(base + SPI_SSPCR1_OFFSET, SPI_SSPCR1_SSE);
    
    *spi = state;
    return 0;
}

uint32_t hal_spi_set_clock(hal_spi_t spi, uint32_t clock_hz) {
    spi_state_t* state = (spi_state_t*)spi;
    
    /* SSPCLK = sys_clk / (CPSDVSR * (1 + SCR)) */
    /* CPSDVSR must be even, 2-254 */
    /* SCR is 0-255 */
    
    uint32_t prescale = 2;
    while (prescale <= 254) {
        uint32_t scr = (s_sys_clock_hz / (prescale * clock_hz)) - 1;
        if (scr <= 255) {
            reg_write(state->base + SPI_SSPCPSR_OFFSET, prescale);
            uint32_t cr0 = reg_read(state->base + SPI_SSPCR0_OFFSET);
            cr0 = (cr0 & 0xFF) | (scr << 8);
            reg_write(state->base + SPI_SSPCR0_OFFSET, cr0);
            state->clock_hz = s_sys_clock_hz / (prescale * (scr + 1));
            return state->clock_hz;
        }
        prescale += 2;
    }
    
    /* Couldn't achieve requested frequency */
    return 0;
}

int hal_spi_transfer(hal_spi_t spi, const uint8_t* tx, uint8_t* rx, uint32_t len) {
    spi_state_t* state = (spi_state_t*)spi;
    uint32_t base = state->base;
    
    for (uint32_t i = 0; i < len; i++) {
        /* Wait for TX FIFO not full */
        while (reg_read(base + SPI_SSPSR_OFFSET) & SPI_SSPSR_BSY) {}
        
        /* Write data */
        uint8_t out = tx ? tx[i] : 0xFF;
        reg_write(base + SPI_SSPDR_OFFSET, out);
        
        /* Wait for RX data */
        while (!(reg_read(base + SPI_SSPSR_OFFSET) & SPI_SSPSR_RNE)) {}
        
        /* Read data */
        uint8_t in = reg_read(base + SPI_SSPDR_OFFSET) & 0xFF;
        if (rx) rx[i] = in;
    }
    
    return 0;
}

/*============================================================================
 * Storage - delegates to SD card driver
 *============================================================================*/

/* Forward declarations - implemented in sd_spi.c */
extern int sd_init(void);
extern int sd_read_blocks(uint32_t block, uint8_t* buffer, uint32_t count);
extern uint32_t sd_get_block_count(void);

static bool s_storage_initialized = false;
static uint32_t s_sd_block_count = 0;

int hal_storage_init(void) {
    if (s_storage_initialized) return 0;
    
    /* Configure SPI pins for SD card */
    /* CS as GPIO output */
    hal_gpio_set_mode(SD_CS_PIN, HAL_GPIO_OUTPUT);
    hal_gpio_write(SD_CS_PIN, true);  /* CS high (deselected) */
    
    /* SCK, MOSI, MISO as SPI function */
    reg_write(IO_BANK0_BASE + IO_BANK0_GPIO_CTRL(SD_SCK_PIN), 1);   /* SPI */
    reg_write(IO_BANK0_BASE + IO_BANK0_GPIO_CTRL(SD_MOSI_PIN), 1);
    reg_write(IO_BANK0_BASE + IO_BANK0_GPIO_CTRL(SD_MISO_PIN), 1);
    
    /* Configure pads */
    reg_write(PADS_BANK0_BASE + PADS_BANK0_GPIO_OFFSET(SD_SCK_PIN),
        PADS_BANK0_GPIO_IE | PADS_BANK0_GPIO_DRIVE_4MA);
    reg_write(PADS_BANK0_BASE + PADS_BANK0_GPIO_OFFSET(SD_MOSI_PIN),
        PADS_BANK0_GPIO_IE | PADS_BANK0_GPIO_DRIVE_4MA);
    reg_write(PADS_BANK0_BASE + PADS_BANK0_GPIO_OFFSET(SD_MISO_PIN),
        PADS_BANK0_GPIO_IE | PADS_BANK0_GPIO_PUE);  /* Pull-up on MISO */
    
    /* Initialize SPI at slow speed for SD init */
    hal_spi_config_t spi_cfg = {
        .clock_hz = 400000,     /* 400kHz for init */
        .mode = 0,
        .msb_first = true
    };
    
    hal_spi_t spi;
    if (hal_spi_init(&spi, SD_SPI_INST, &spi_cfg) != 0) {
        return -1;
    }
    
    s_storage_initialized = true;
    return 0;
}

int hal_storage_open(hal_storage_t* dev) {
    if (!s_storage_initialized) {
        if (hal_storage_init() != 0) return -1;
    }
    
    /* Initialize SD card */
    if (sd_init() != 0) {
        return -1;
    }
    
    /* Speed up SPI after init */
    hal_spi_set_clock(&s_spi_state[SD_SPI_INST], 25000000);  /* 25MHz */
    
    s_sd_block_count = sd_get_block_count();
    
    *dev = (void*)1;  /* Non-null handle */
    return 0;
}

void hal_storage_close(hal_storage_t dev) {
    (void)dev;
    /* Nothing to do for SD */
}

int hal_storage_info(hal_storage_t dev, hal_storage_info_t* info) {
    (void)dev;
    info->sector_size = 512;
    info->sector_count = s_sd_block_count;
    info->total_size = s_sd_block_count * 512;
    info->readonly = false;
    info->name = "SD Card";
    return 0;
}

int32_t hal_storage_read(hal_storage_t dev, uint32_t offset, void* buffer, uint32_t size) {
    (void)dev;
    
    uint8_t* buf = (uint8_t*)buffer;
    uint32_t block = offset / 512;
    uint32_t block_offset = offset % 512;
    uint32_t bytes_read = 0;
    uint8_t temp_block[512];
    
    while (bytes_read < size) {
        /* Read block */
        if (sd_read_blocks(block, temp_block, 1) != 0) {
            return -1;
        }
        
        /* Copy relevant portion */
        uint32_t copy_start = (bytes_read == 0) ? block_offset : 0;
        uint32_t copy_len = 512 - copy_start;
        if (copy_len > (size - bytes_read)) {
            copy_len = size - bytes_read;
        }
        
        for (uint32_t i = 0; i < copy_len; i++) {
            buf[bytes_read + i] = temp_block[copy_start + i];
        }
        
        bytes_read += copy_len;
        block++;
    }
    
    return bytes_read;
}

/*============================================================================
 * System Control
 *============================================================================*/

__attribute__((noreturn))
void hal_system_reset(void) {
    /* Use watchdog for reset */
    reg_write(WATCHDOG_BASE + WATCHDOG_CTRL_OFFSET, 
        WATCHDOG_CTRL_TRIGGER);
    
    while (1) {
        __asm__ volatile ("wfi");
    }
}

__attribute__((noreturn))
void hal_system_halt(void) {
    /* Blink LED forever */
    hal_gpio_set_mode(LED_PIN, HAL_GPIO_OUTPUT);
    
    while (1) {
        hal_led_blink(3, 100, 100);
        hal_delay_ms(1000);
    }
}

/*============================================================================
 * LED
 *============================================================================*/

void hal_led_set(bool on) {
    static bool led_initialized = false;
    if (!led_initialized) {
        hal_gpio_set_mode(LED_PIN, HAL_GPIO_OUTPUT);
        led_initialized = true;
    }
    hal_gpio_write(LED_PIN, on);
}

void hal_led_blink(uint32_t count, uint32_t on_ms, uint32_t off_ms) {
    for (uint32_t i = 0; i < count; i++) {
        hal_led_set(true);
        hal_delay_ms(on_ms);
        hal_led_set(false);
        hal_delay_ms(off_ms);
    }
}
