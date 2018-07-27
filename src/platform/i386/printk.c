#include "string.h"
#include "kernel.h"
#include "printk.h"

#define PRINTK_BUFFER 1024

static void (*printk_handlers[PRINTK_MAX_HANDLERS])(const char *);
static unsigned num_handlers = 0;

int
printk_register_handler(printk_t type, void (*handler)(const char *))
{
	if (type > PRINTK_MAX_HANDLERS || handler == NULL) return -1;

	printk_handlers[type] = handler;

	return 0;
}

void
printk(const char *fmt, ...)
{
	char         buffer[PRINTK_BUFFER];
	va_list      args;
	unsigned int l, i;

	va_start(args, fmt);
	l = vsprintf(buffer, fmt, args);
	va_end(args);

//	for (i = 0; i < PRINTK_MAX_HANDLERS; i++) {
//		if (printk_handlers[i]) printk_handlers[i](buffer);
//	}
	if (printk_handlers[PRINTK_VGA]) printk_handlers[PRINTK_VGA](buffer);
}
