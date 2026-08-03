#include "mss_uart.h"

/* PolarFire SoC System Register Subsystem (SYSREG) Address Maps */
#define SYSREG_BASE          0x20002000UL
#define SYSREG_SUBBLK_CLK    (*(volatile unsigned int*)(SYSREG_BASE + 0x08))
#define SYSREG_SUBBLK_RST    (*(volatile unsigned int*)(SYSREG_BASE + 0x0C))

/* MMUART0 Peripheral Base Address */
#define MMUART0_BASE         0x20000000UL

/* MMUART0 Core Registers */
#define UART_RBR_THR_DLL     (*(volatile unsigned int*)(MMUART0_BASE + 0x00))
#define UART_IER_DLM         (*(volatile unsigned int*)(MMUART0_BASE + 0x04))
#define UART_FCR             (*(volatile unsigned int*)(MMUART0_BASE + 0x08))
#define UART_LCR             (*(volatile unsigned int*)(MMUART0_BASE + 0x0C))
#define UART_LSR             (*(volatile unsigned int*)(MMUART0_BASE + 0x14))

/* Bit position 5 corresponds to MMUART0 inside the Sub-Block Control Registers */
#define SUBBLK_MMUART0_BIT   (1U << 5)

void uart_init(void) {
    // -------------------------------------------------------------------------
    // Hardware Clock and Reset Provisioning (Replacing Microchip HAL)
    // -------------------------------------------------------------------------
    
    // 1. Enable the peripheral clock tree for MMUART0
    SYSREG_SUBBLK_CLK |= SUBBLK_MMUART0_BIT;

    // 2. Assert hardware reset line to clear out previous peripheral state
    SYSREG_SUBBLK_RST |= SUBBLK_MMUART0_BIT;
    
    // Small baseline loop delay to guarantee reset latch setup time
    volatile unsigned int delay = 100U;
    while(delay--);

    // 3. De-assert reset line to release the MMUART0 block into operation
    SYSREG_SUBBLK_RST &= ~SUBBLK_MMUART0_BIT;

     // -------------------------------------------------------------------------
    // Core UART Protocol Formatting Layout Configurations
    // -------------------------------------------------------------------------

    // 4. Disable all UART interrupts globally for boot stability
    UART_IER_DLM = 0x00;

    // 5. Enable Divisor Latch Access Bit (DLAB) to unlock baud rate parameters
    UART_LCR = 0x80;


    // 6. Set Baud Rate Divisor to 43 (0x002B) for 115200 baud @ 80 MHz default clock
    UART_RBR_THR_DLL = 0x2B; // Lower 8 bits of divisor (0x2B)
    UART_IER_DLM     = 0x00; // Upper 8 bits of divisor (0x00)


    // Set Baud Rate Divisor to 81 (0x0051) for 115200 baud @ 150MHz pclk
    //UART_RBR_THR_DLL = 0x51; 
    //UART_IER_DLM     = 0x00; 

    // Clear DLAB and configure line: 8 Data Bits, 1 Stop Bit, No Parity (8N1)
    UART_LCR = 0x03;

    // Enable and clear Transmitter/Receiver hardware FIFO buffers
    UART_FCR = 0x07; 
}

void uart_putc(char c) {
    while (!(UART_LSR & 0x20)); // Wait until THRE bit is set
    UART_RBR_THR_DLL = c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

char uart_getc(void) {
    while (!(UART_LSR & 0x01)); // Wait until Data Ready (DR) bit is set
    return (char)UART_RBR_THR_DLL;
}

int check_command(const char *expected) {
    for (int i = 0; expected[i] != '\0'; i++) {
        if (uart_getc() != expected[i]) {
            return 0;
        }
    }
    return 1;
}