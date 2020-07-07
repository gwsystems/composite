#define ENABLE_SERIAL

#include "string.h"
#include "kernel.h"

/* UART peripheral address */
#define CAV7_UART_CONTROL (*((volatile unsigned long *)(0xE0001000)))
#define CAV7_UART_MODE (*((volatile unsigned long *)(0xE0001004)))
#define CAV7_UART_BRGEN (*((volatile unsigned long *)(0xE0001018)))
#define CAV7_UART_STATUS (*((volatile unsigned long *)(0xE000102C)))
#define CAV7_UART_FIFO (*((volatile unsigned long *)(0xE0001030)))
#define CAV7_UART_BRDIV (*((volatile unsigned long *)(0xE0001034)))
#define CAV7_UART_STATUS_TXE (1U << 3)


void serial_puts(const char *s);

static inline char
serial_recv(void)
{
}

static inline void
serial_send(char out)
{
	if (out == '\n') {
		while ((CAV7_UART_STATUS & CAV7_UART_STATUS_TXE) == 0)
			;
		CAV7_UART_FIFO = '\r';
	}

	while ((CAV7_UART_STATUS & CAV7_UART_STATUS_TXE) == 0)
		;
	CAV7_UART_FIFO = (out);
}

void
serial_puts(const char *s)
{
	for (; *s != '\0'; s++) serial_send(*s);
}

int
serial_handler(struct pt_regs *r)
{
	return 0;
}

void
serial_init(void)
{
	printk_register_handler(serial_puts);
	serial_puts("Serial usable.\r\n");
}
