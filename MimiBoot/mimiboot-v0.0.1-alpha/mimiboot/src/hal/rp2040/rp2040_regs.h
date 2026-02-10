/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * rp2040_regs.h - RP2040 Hardware Register Definitions
 * 
 * Direct register definitions for RP2040 peripherals.
 * These match the RP2040 datasheet exactly.
 */

#ifndef MIMIBOOT_RP2040_REGS_H
#define MIMIBOOT_RP2040_REGS_H

#include <stdint.h>

/*============================================================================
 * Memory Map
 *============================================================================*/

#define ROM_BASE            0x00000000
#define XIP_BASE            0x10000000
#define XIP_MAIN_BASE       0x10000000
#define SRAM_BASE           0x20000000
#define SRAM_STRIPED_BASE   0x20000000
#define SRAM_STRIPED_END    0x20040000

#define APB_BASE            0x40000000
#define AHB_BASE            0x50000000
#define IOPORT_BASE         0xD0000000
#define CORTEX_M_BASE       0xE0000000

/*============================================================================
 * Register Aliases (atomic access)
 *============================================================================*/

#define REG_ALIAS_NORMAL    0x0000
#define REG_ALIAS_XOR_BITS  0x1000
#define REG_ALIAS_SET_BITS  0x2000
#define REG_ALIAS_CLR_BITS  0x3000

/*============================================================================
 * Resets
 *============================================================================*/

#define RESETS_BASE         0x4000C000

#define RESETS_RESET_OFFSET         0x00
#define RESETS_WDSEL_OFFSET         0x04
#define RESETS_RESET_DONE_OFFSET    0x08

/* Reset bit indices */
#define RESET_ADC           0
#define RESET_BUSCTRL       1
#define RESET_DMA           2
#define RESET_I2C0          3
#define RESET_I2C1          4
#define RESET_IO_BANK0      5
#define RESET_IO_QSPI       6
#define RESET_JTAG          7
#define RESET_PADS_BANK0    8
#define RESET_PADS_QSPI     9
#define RESET_PIO0          10
#define RESET_PIO1          11
#define RESET_PLL_SYS       12
#define RESET_PLL_USB       13
#define RESET_PWM           14
#define RESET_RTC           15
#define RESET_SPI0          16
#define RESET_SPI1          17
#define RESET_SYSCFG        18
#define RESET_SYSINFO       19
#define RESET_TBMAN         20
#define RESET_TIMER         21
#define RESET_UART0         22
#define RESET_UART1         23
#define RESET_USBCTRL       24

/*============================================================================
 * IO Bank 0 (GPIO control)
 *============================================================================*/

#define IO_BANK0_BASE       0x40014000

/* GPIO control register offset for pin n */
#define IO_BANK0_GPIO_STATUS(n) (0x000 + (n) * 8)
#define IO_BANK0_GPIO_CTRL(n)   (0x004 + (n) * 8)

/* CTRL register fields */
#define IO_BANK0_GPIO_CTRL_FUNCSEL_MASK     0x1F
#define IO_BANK0_GPIO_CTRL_OUTOVER_MASK     (0x3 << 8)
#define IO_BANK0_GPIO_CTRL_OEOVER_MASK      (0x3 << 12)
#define IO_BANK0_GPIO_CTRL_INOVER_MASK      (0x3 << 16)
#define IO_BANK0_GPIO_CTRL_IRQOVER_MASK     (0x3 << 28)

/* Function select values */
#define GPIO_FUNC_XIP       0
#define GPIO_FUNC_SPI       1
#define GPIO_FUNC_UART      2
#define GPIO_FUNC_I2C       3
#define GPIO_FUNC_PWM       4
#define GPIO_FUNC_SIO       5
#define GPIO_FUNC_PIO0      6
#define GPIO_FUNC_PIO1      7
#define GPIO_FUNC_GPCK      8
#define GPIO_FUNC_USB       9
#define GPIO_FUNC_NULL      0x1F

/*============================================================================
 * Pads Bank 0
 *============================================================================*/

#define PADS_BANK0_BASE     0x4001C000

#define PADS_BANK0_VOLTAGE_SELECT_OFFSET    0x00
#define PADS_BANK0_GPIO_OFFSET(n)           (0x04 + (n) * 4)

/* Pad control fields */
#define PADS_BANK0_GPIO_OD_DISABLE  (1 << 7)    /* Output disable */
#define PADS_BANK0_GPIO_IE          (1 << 6)    /* Input enable */
#define PADS_BANK0_GPIO_DRIVE_2MA   (0 << 4)
#define PADS_BANK0_GPIO_DRIVE_4MA   (1 << 4)
#define PADS_BANK0_GPIO_DRIVE_8MA   (2 << 4)
#define PADS_BANK0_GPIO_DRIVE_12MA  (3 << 4)
#define PADS_BANK0_GPIO_PUE         (1 << 3)    /* Pull-up enable */
#define PADS_BANK0_GPIO_PDE         (1 << 2)    /* Pull-down enable */
#define PADS_BANK0_GPIO_SCHMITT     (1 << 1)    /* Schmitt trigger */
#define PADS_BANK0_GPIO_SLEWFAST    (1 << 0)    /* Slew rate fast */

/*============================================================================
 * SIO (Single-cycle IO)
 *============================================================================*/

#define SIO_BASE            0xD0000000

#define SIO_CPUID_OFFSET            0x00
#define SIO_GPIO_IN_OFFSET          0x04
#define SIO_GPIO_HI_IN_OFFSET       0x08
#define SIO_GPIO_OUT_OFFSET         0x10
#define SIO_GPIO_OUT_SET_OFFSET     0x14
#define SIO_GPIO_OUT_CLR_OFFSET     0x18
#define SIO_GPIO_OUT_XOR_OFFSET     0x1C
#define SIO_GPIO_OE_OFFSET          0x20
#define SIO_GPIO_OE_SET_OFFSET      0x24
#define SIO_GPIO_OE_CLR_OFFSET      0x28
#define SIO_GPIO_OE_XOR_OFFSET      0x2C

/*============================================================================
 * UART
 *============================================================================*/

#define UART0_BASE          0x40034000
#define UART1_BASE          0x40038000

#define UART_DR_OFFSET      0x00    /* Data register */
#define UART_RSR_OFFSET     0x04    /* Receive status */
#define UART_FR_OFFSET      0x18    /* Flag register */
#define UART_ILPR_OFFSET    0x20    /* IrDA low-power counter */
#define UART_IBRD_OFFSET    0x24    /* Integer baud rate */
#define UART_FBRD_OFFSET    0x28    /* Fractional baud rate */
#define UART_LCR_H_OFFSET   0x2C    /* Line control */
#define UART_CR_OFFSET      0x30    /* Control register */
#define UART_IFLS_OFFSET    0x34    /* Interrupt FIFO level select */
#define UART_IMSC_OFFSET    0x38    /* Interrupt mask */
#define UART_RIS_OFFSET     0x3C    /* Raw interrupt status */
#define UART_MIS_OFFSET     0x40    /* Masked interrupt status */
#define UART_ICR_OFFSET     0x44    /* Interrupt clear */
#define UART_DMACR_OFFSET   0x48    /* DMA control */

/* Flag register bits */
#define UART_FR_TXFE        (1 << 7)    /* TX FIFO empty */
#define UART_FR_RXFF        (1 << 6)    /* RX FIFO full */
#define UART_FR_TXFF        (1 << 5)    /* TX FIFO full */
#define UART_FR_RXFE        (1 << 4)    /* RX FIFO empty */
#define UART_FR_BUSY        (1 << 3)    /* UART busy */

/* Line control bits */
#define UART_LCR_H_SPS      (1 << 7)    /* Stick parity select */
#define UART_LCR_H_WLEN_5   (0 << 5)
#define UART_LCR_H_WLEN_6   (1 << 5)
#define UART_LCR_H_WLEN_7   (2 << 5)
#define UART_LCR_H_WLEN_8   (3 << 5)
#define UART_LCR_H_FEN      (1 << 4)    /* FIFO enable */
#define UART_LCR_H_STP2     (1 << 3)    /* Two stop bits */
#define UART_LCR_H_EPS      (1 << 2)    /* Even parity select */
#define UART_LCR_H_PEN      (1 << 1)    /* Parity enable */
#define UART_LCR_H_BRK      (1 << 0)    /* Send break */

/* Control register bits */
#define UART_CR_CTSEN       (1 << 15)
#define UART_CR_RTSEN       (1 << 14)
#define UART_CR_RTS         (1 << 11)
#define UART_CR_RXE         (1 << 9)    /* Receive enable */
#define UART_CR_TXE         (1 << 8)    /* Transmit enable */
#define UART_CR_LBE         (1 << 7)    /* Loopback enable */
#define UART_CR_UARTEN      (1 << 0)    /* UART enable */

/*============================================================================
 * SPI
 *============================================================================*/

#define SPI0_BASE           0x4003C000
#define SPI1_BASE           0x40040000

#define SPI_SSPCR0_OFFSET   0x00    /* Control register 0 */
#define SPI_SSPCR1_OFFSET   0x04    /* Control register 1 */
#define SPI_SSPDR_OFFSET    0x08    /* Data register */
#define SPI_SSPSR_OFFSET    0x0C    /* Status register */
#define SPI_SSPCPSR_OFFSET  0x10    /* Clock prescale */
#define SPI_SSPIMSC_OFFSET  0x14    /* Interrupt mask */
#define SPI_SSPRIS_OFFSET   0x18    /* Raw interrupt status */
#define SPI_SSPMIS_OFFSET   0x1C    /* Masked interrupt status */
#define SPI_SSPICR_OFFSET   0x20    /* Interrupt clear */
#define SPI_SSPDMACR_OFFSET 0x24    /* DMA control */

/* CR0 bits */
#define SPI_SSPCR0_SCR_SHIFT    8   /* Serial clock rate */
#define SPI_SSPCR0_SPH          (1 << 7)    /* Clock phase */
#define SPI_SSPCR0_SPO          (1 << 6)    /* Clock polarity */
#define SPI_SSPCR0_FRF_MOTO     (0 << 4)    /* Motorola SPI */
#define SPI_SSPCR0_FRF_TI       (1 << 4)    /* TI sync serial */
#define SPI_SSPCR0_FRF_NM       (2 << 4)    /* National Microwire */
#define SPI_SSPCR0_DSS_MASK     0xF         /* Data size select */

/* CR1 bits */
#define SPI_SSPCR1_SOD          (1 << 3)    /* Slave output disable */
#define SPI_SSPCR1_MS           (1 << 2)    /* Master/slave select */
#define SPI_SSPCR1_SSE          (1 << 1)    /* SSP enable */
#define SPI_SSPCR1_LBM          (1 << 0)    /* Loopback mode */

/* Status bits */
#define SPI_SSPSR_BSY           (1 << 4)    /* Busy */
#define SPI_SSPSR_RFF           (1 << 3)    /* RX FIFO full */
#define SPI_SSPSR_RNE           (1 << 2)    /* RX FIFO not empty */
#define SPI_SSPSR_TNF           (1 << 1)    /* TX FIFO not full */
#define SPI_SSPSR_TFE           (1 << 0)    /* TX FIFO empty */

/*============================================================================
 * Timer
 *============================================================================*/

#define TIMER_BASE          0x40054000

#define TIMER_TIMEHW_OFFSET     0x00    /* Time high (write) */
#define TIMER_TIMELW_OFFSET     0x04    /* Time low (write) */
#define TIMER_TIMEHR_OFFSET     0x08    /* Time high (read) */
#define TIMER_TIMELR_OFFSET     0x0C    /* Time low (read) */
#define TIMER_ALARM0_OFFSET     0x10
#define TIMER_ALARM1_OFFSET     0x14
#define TIMER_ALARM2_OFFSET     0x18
#define TIMER_ALARM3_OFFSET     0x1C
#define TIMER_ARMED_OFFSET      0x20
#define TIMER_TIMERAWH_OFFSET   0x24
#define TIMER_TIMERAWL_OFFSET   0x28
#define TIMER_DBGPAUSE_OFFSET   0x2C
#define TIMER_PAUSE_OFFSET      0x30
#define TIMER_INTR_OFFSET       0x34
#define TIMER_INTE_OFFSET       0x38
#define TIMER_INTF_OFFSET       0x3C
#define TIMER_INTS_OFFSET       0x40

/*============================================================================
 * Watchdog
 *============================================================================*/

#define WATCHDOG_BASE       0x40058000

#define WATCHDOG_CTRL_OFFSET        0x00
#define WATCHDOG_LOAD_OFFSET        0x04
#define WATCHDOG_REASON_OFFSET      0x08
#define WATCHDOG_SCRATCH0_OFFSET    0x0C
#define WATCHDOG_SCRATCH1_OFFSET    0x10
#define WATCHDOG_SCRATCH2_OFFSET    0x14
#define WATCHDOG_SCRATCH3_OFFSET    0x18
#define WATCHDOG_SCRATCH4_OFFSET    0x1C
#define WATCHDOG_SCRATCH5_OFFSET    0x20
#define WATCHDOG_SCRATCH6_OFFSET    0x24
#define WATCHDOG_SCRATCH7_OFFSET    0x28
#define WATCHDOG_TICK_OFFSET        0x2C

/* CTRL bits */
#define WATCHDOG_CTRL_TRIGGER       (1 << 31)
#define WATCHDOG_CTRL_ENABLE        (1 << 30)
#define WATCHDOG_CTRL_PAUSE_DBG1    (1 << 26)
#define WATCHDOG_CTRL_PAUSE_DBG0    (1 << 25)
#define WATCHDOG_CTRL_PAUSE_JTAG    (1 << 24)

/*============================================================================
 * PSM (Power-on State Machine) / VREG_AND_CHIP_RESET
 *============================================================================*/

#define VREG_AND_CHIP_RESET_BASE    0x40064000

#define CHIP_RESET_OFFSET           0x00

/*============================================================================
 * Cortex-M0+ Core Registers
 *============================================================================*/

#define PPB_BASE            0xE0000000

/* SysTick */
#define SYST_CSR            (PPB_BASE + 0xE010)
#define SYST_RVR            (PPB_BASE + 0xE014)
#define SYST_CVR            (PPB_BASE + 0xE018)
#define SYST_CALIB          (PPB_BASE + 0xE01C)

/* NVIC */
#define NVIC_ISER           (PPB_BASE + 0xE100)
#define NVIC_ICER           (PPB_BASE + 0xE180)
#define NVIC_ISPR           (PPB_BASE + 0xE200)
#define NVIC_ICPR           (PPB_BASE + 0xE280)
#define NVIC_IPR0           (PPB_BASE + 0xE400)

/* SCB */
#define SCB_CPUID           (PPB_BASE + 0xED00)
#define SCB_ICSR            (PPB_BASE + 0xED04)
#define SCB_VTOR            (PPB_BASE + 0xED08)
#define SCB_AIRCR           (PPB_BASE + 0xED0C)
#define SCB_SCR             (PPB_BASE + 0xED10)

#define SCB_AIRCR_VECTKEY   0x05FA0000
#define SCB_AIRCR_SYSRESETREQ (1 << 2)

#endif /* MIMIBOOT_RP2040_REGS_H */
