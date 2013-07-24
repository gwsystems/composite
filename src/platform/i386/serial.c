#include "string.h"
#include "ports.h"
#include "isr.h"
#include "serial.h"
#include "printk.h"

extern int keep_kernel_running;

enum serial_ports {
    SERIAL_PORT_A = 0x3F8,
    SERIAL_PORT_B = 0x2F8,
    SERIAL_PORT_C = 0x3E8,
    SERIAL_PORT_D = 0x2E8
};

static int
serial_rcvd(void) 
{
	return inb(SERIAL_PORT_A + 5) & 1;
}

static char
serial_recv(void) 
{
	if (serial_rcvd() == 0)
        return '\0';

	return inb(SERIAL_PORT_A);
}

static int
serial_transmit_empty(void) 
{
	return inb(SERIAL_PORT_A + 5) & 0x20;
}

static void
serial_send(char out) 
{
	while (serial_transmit_empty() == 0);
	outb(SERIAL_PORT_A, out);
}

void
serial__puts(const char *s) 
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
	case 27:
		keep_kernel_running = 0;
		break;
        case '\0':
            return;
            break;
		case 127:
			serial = 0x08;
			break;
		case 13:
			serial = '\n';
			break;
		default:
			break;
	}

     printk(INFO, "Serial: %d\n", serial); 
	//printk(RAW, "%c", serial);
}

void
serial__init(void) 
{
    printk(INFO, "Registering Serial IRQ\n");
    register_interrupt_handler(IRQ4, &serial_handler);

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

