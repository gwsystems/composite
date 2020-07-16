#ifndef BOARD_SPECIFICS_H
#define BOARD_SPECIFICS_H

#if defined(ZYNQ_ZC702)
#define BOOT_SRAM_HEAP 0x80001000
#define USER_VADDR_ARBITRARY 0x1A000000
#define BOOT_SRAM_PTE 14
#define BOOT_SRAM_UVM_PTE  16
#define KERN_VADDR 0x80010000
#define USER_VADDR (USER_VADDR_ARBITRARY + 0x10000)
#define KERN_VADDR_ANOTHER_ARBITRARY 0x9F000000
#define USER_VADDR_PAGES_ARBITRARY (13 * 256)

#define CAV7_UART_CONTROL (*((volatile unsigned long *)(0xE0001000)))
#define CAV7_UART_MODE (*((volatile unsigned long *)(0xE0001004)))
#define CAV7_UART_BRGEN (*((volatile unsigned long *)(0xE0001018)))
#define CAV7_UART_STATUS (*((volatile unsigned long *)(0xE000102C)))
#define CAV7_UART_FIFO (*((volatile unsigned long *)(0xE0001030)))
#define CAV7_UART_BRDIV (*((volatile unsigned long *)(0xE0001034)))
#define CAV7_UART_STATUS_TXE (1U << 3)


#define KERN_END       0x10000000
#define MOD_START      0x90000000
#define MOD_END        (MOD_START + 0x4000000)
#define BOOTC_ENTRY    0x10000000
#define BOOTC_VADDR    0x00400000
#define KERN_BOOT_HEAP 0x98000000
#define KERN_MEM_END   0xBFFF0000

#endif

#endif /* BOARD_SPECIFICS_H */
