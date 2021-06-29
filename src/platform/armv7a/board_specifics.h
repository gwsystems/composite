#ifndef BOARD_SPECIFICS_H
#define BOARD_SPECIFICS_H

#if defined(ZYNQ_ZC702)
#define CAV7_UART_CONTROL (*((volatile unsigned long *)(0xE0001000)))
#define CAV7_UART_MODE (*((volatile unsigned long *)(0xE0001004)))
#define CAV7_UART_BRGEN (*((volatile unsigned long *)(0xE0001018)))
#define CAV7_UART_STATUS (*((volatile unsigned long *)(0xE000102C)))
#define CAV7_UART_FIFO (*((volatile unsigned long *)(0xE0001030)))
#define CAV7_UART_BRDIV (*((volatile unsigned long *)(0xE0001034)))
#define CAV7_UART_STATUS_TXE (1U << 3)

#define KERN_MEM_END 0xBFFF0000

#endif

#endif /* BOARD_SPECIFICS_H */
