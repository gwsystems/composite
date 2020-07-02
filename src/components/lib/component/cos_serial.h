#ifndef COS_SERIAL_H
#define COS_SERIAL_H

#include <cos_io.h>

/* code duplicated from platform/i386/serial.c */
enum cos_serial_ports
{
	COS_SERIAL_PORT_A = 0x3F8,
	COS_SERIAL_PORT_B = 0x2F8,
	COS_SERIAL_PORT_C = 0x3E8,
	COS_SERIAL_PORT_D = 0x2E8
};

static inline void
cos_serial_putc(char out)
{
	while ((inb(COS_SERIAL_PORT_A + 5) & 0x20) == 0) {
		/* wait for port to be ready to send */
	}
	outb(COS_SERIAL_PORT_A, out);
}

/* NOTE: can be interleaved & the output could just look like garbage. */
static inline void
cos_serial_puts(const char *s)
{
	for (; *s != '\0'; s++) cos_serial_putc(*s);
}

/* for binary printing */
static inline void
cos_serial_putb(const char *s, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) cos_serial_putc(*(s + i));
}

#endif /* COS_SERIAL_H */
