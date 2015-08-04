#define ENABLE_SERIAL

#include "string.h"
#include "io.h"
#include "isr.h"
#include "kernel.h"

void serial_puts(const char *s);

enum serial_ports {
	SERIAL_PORT_A = 0x3F8,
	SERIAL_PORT_B = 0x2F8,
	SERIAL_PORT_C = 0x3E8,
	SERIAL_PORT_D = 0x2E8
};

static inline char
serial_recv(void) 
{
	if ((inb(SERIAL_PORT_A + 5) & 1) == 0)
        	return '\0';
	return inb(SERIAL_PORT_A);
}

static inline void
serial_send(char out) 
{
	while ((inb(SERIAL_PORT_A + 5) & 0x20) == 0) {
		/* wait for port to be ready to send */
	}
	outb(SERIAL_PORT_A, out);
}

void
serial_puts(const char *s) 
{
	for (; *s != '\0'; s++)
		serial_send(*s);
}

static void
serial_handler(struct registers *r)
{
	char serial = serial_recv();

	/*
	 * Fix the serial input assuming it is ascii
	 */
	switch (serial) {
		case '\0':
			return;
		case 127:
			serial = 0x08;
			break;
		case 13:
			serial = '\n';
			break;
		case 3: /* FIXME: Obviously remove this once we have working components */
			die("Break\n");
		default:
			break;
	}

	printk("Serial: %d\n", serial); 
	//printk("%c", serial);
}

void
serial_init(void) 
{
	printk("Enabling serial I/O\n");
	register_interrupt_handler(IRQ4, serial_handler);
	printk_register_handler(serial_puts);

	/* We will initialize the first serial port */
	outb(SERIAL_PORT_A + 1, 0x00);
	outb(SERIAL_PORT_A + 3, 0x80); /* Enable divisor mode */
	outb(SERIAL_PORT_A + 0, 0x03); /* Div Low:  03 Set the port to 38400 bps */
	outb(SERIAL_PORT_A + 1, 0x00); /* Div High: 00 */
	outb(SERIAL_PORT_A + 3, 0x03);
	outb(SERIAL_PORT_A + 2, 0xC7);
	outb(SERIAL_PORT_A + 4, 0x0B);

	outb(SERIAL_PORT_A + 1, 0x01); /* Enable interrupts on receive */
}
