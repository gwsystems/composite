#define ENABLE_SERIAL

#include "string.h"
#include "kernel.h"
#include "board_specifics.h"

void serial_puts(const char *s);

static inline char
serial_recv(void)
{
}

void
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
