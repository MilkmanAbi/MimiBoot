/**
 * MimiBoot Example Payload - LED Blink
 * 
 * This is a minimal payload that blinks the onboard LED.
 * It demonstrates:
 * - Being loaded into RAM by MimiBoot
 * - Receiving and using the handoff structure
 * - Running without any flash access
 * 
 * Build this, copy kernel.elf to SD card at /boot/kernel.elf,
 * and MimiBoot will load and run it.
 */

#include <stdint.h>
#include <stdbool.h>
#include "mimiboot/handoff.h"

/*============================================================================
 * RP2040 Register Definitions (minimal set)
 *============================================================================*/

#define SIO_BASE            0xD0000000
#define SIO_GPIO_OUT_SET    (*(volatile uint32_t*)(SIO_BASE + 0x14))
#define SIO_GPIO_OUT_CLR    (*(volatile uint32_t*)(SIO_BASE + 0x18))
#define SIO_GPIO_OE_SET     (*(volatile uint32_t*)(SIO_BASE + 0x24))

#define IO_BANK0_BASE       0x40014000
#define PADS_BANK0_BASE     0x4001C000
#define RESETS_BASE         0x4000C000
#define TIMER_BASE          0x40054000

#define LED_PIN             25

/*============================================================================
 * Minimal Hardware Access
 *============================================================================*/

static void delay_ms(uint32_t ms) {
    /* Read timer (1MHz on RP2040) */
    volatile uint32_t* timer_lo = (volatile uint32_t*)(TIMER_BASE + 0x0C);
    uint32_t start = *timer_lo;
    uint32_t target = ms * 1000;
    
    while ((*timer_lo - start) < target) {
        __asm__ volatile ("nop");
    }
}

static void gpio_init(uint32_t pin) {
    /* Configure GPIO for SIO function (5) */
    volatile uint32_t* gpio_ctrl = (volatile uint32_t*)(IO_BANK0_BASE + 0x04 + pin * 8);
    *gpio_ctrl = 5;
    
    /* Configure pad */
    volatile uint32_t* pad_ctrl = (volatile uint32_t*)(PADS_BANK0_BASE + 0x04 + pin * 4);
    *pad_ctrl = (1 << 6);  /* Input enable */
    
    /* Enable output */
    SIO_GPIO_OE_SET = (1 << pin);
}

static void gpio_set(uint32_t pin, bool state) {
    if (state) {
        SIO_GPIO_OUT_SET = (1 << pin);
    } else {
        SIO_GPIO_OUT_CLR = (1 << pin);
    }
}

/*============================================================================
 * UART Output (minimal)
 *============================================================================*/

#define UART0_BASE          0x40034000
#define UART_DR             (*(volatile uint32_t*)(UART0_BASE + 0x00))
#define UART_FR             (*(volatile uint32_t*)(UART0_BASE + 0x18))
#define UART_FR_TXFF        (1 << 5)

static void uart_putc(char c) {
    while (UART_FR & UART_FR_TXFF) {}
    UART_DR = c;
}

static void uart_puts(const char* s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static void uart_puthex(uint32_t val) {
    const char* hex = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc(hex[(val >> i) & 0xF]);
    }
}

/*============================================================================
 * Main Entry Point
 *============================================================================*/

/**
 * Payload entry point.
 * 
 * Called by MimiBoot with handoff structure pointer in r0.
 * We're running entirely from RAM at this point.
 */
void payload_main(mimi_handoff_t* handoff) {
    /* Initialize LED GPIO */
    gpio_init(LED_PIN);
    
    /* Print banner */
    uart_puts("\n\n");
    uart_puts("========================================\n");
    uart_puts("  Payload: LED Blink\n");
    uart_puts("  Loaded by MimiBoot\n");
    uart_puts("========================================\n\n");
    
    /* Check handoff structure */
    if (handoff != NULL && handoff->magic == MIMI_HANDOFF_MAGIC) {
        uart_puts("Handoff received!\n");
        uart_puts("  Version: ");
        uart_puthex(handoff->version);
        uart_puts("\n");
        
        uart_puts("  Boot time: ");
        uart_puthex(handoff->boot_time_us);
        uart_puts(" us\n");
        
        uart_puts("  System clock: ");
        uart_puthex(handoff->sys_clock_hz);
        uart_puts(" Hz\n");
        
        uart_puts("  RAM: ");
        uart_puthex(handoff->ram_base);
        uart_puts(" - ");
        uart_puthex(handoff->ram_base + handoff->ram_size);
        uart_puts("\n");
        
        uart_puts("  Image: ");
        uart_puts(handoff->image.name);
        uart_puts("\n");
        
        uart_puts("  Entry: ");
        uart_puthex(handoff->image.entry);
        uart_puts("\n");
        
    } else {
        uart_puts("No handoff structure (booted directly?)\n");
    }
    
    uart_puts("\nBlinking LED forever...\n\n");
    
    /* Blink LED forever */
    uint32_t count = 0;
    while (1) {
        gpio_set(LED_PIN, true);
        delay_ms(250);
        gpio_set(LED_PIN, false);
        delay_ms(250);
        
        count++;
        if ((count % 10) == 0) {
            uart_puts("Blink count: ");
            uart_puthex(count);
            uart_puts("\n");
        }
    }
}
