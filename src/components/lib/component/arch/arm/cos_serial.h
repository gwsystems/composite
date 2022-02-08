#ifndef COS_SERIAL_CAV7_H
#define COS_SERIAL_CAV7_H

/* Phani: Not sure if this is accessible at userlevel! */
/* UART peripheral address */
#define CAV7_UART_CONTROL (*((volatile unsigned long *)(0xE0001000)))
#define CAV7_UART_MODE (*((volatile unsigned long *)(0xE0001004)))
#define CAV7_UART_BRGEN (*((volatile unsigned long *)(0xE0001018)))
#define CAV7_UART_STATUS (*((volatile unsigned long *)(0xE000102C)))
#define CAV7_UART_FIFO (*((volatile unsigned long *)(0xE0001030)))
#define CAV7_UART_BRDIV (*((volatile unsigned long *)(0xE0001034)))
#define CAV7_UART_STATUS_TXE (1U << 3)

static inline void
cos_serial_putc(char out)
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

#endif /* COS_SERIAL_CAV7_H */
