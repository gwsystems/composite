#include "string.h"
#include "io.h"
#include "kernel.h"

#define PRINTK_BUFFER 1024
#define MAX_HANDLERS 5

static void (*printk_handlers[MAX_HANDLERS])(const char *);
static unsigned num_handlers = 0;

const char const * log_level_str[] = {
    [RAW]       = "",
    [DEBUG]	= "DEBUG",
    [INFO]      = "INFO",
    [WARN]      = "WARN",
    [ERROR]     = "ERROR",
    [CRITICAL]  = "CRITICAL",
};

int
printk_register_handler(void (*handler)(const char *))
{
	if (handler == NULL || num_handlers > (sizeof(printk_handlers) / sizeof(printk_handlers[0])))
		return -1;
	printk_handlers[num_handlers++] = handler;
	return 0;
}

void 
printk(enum log_level level, const char *fmt, ...)
{
	char buffer[PRINTK_BUFFER];
	va_list args;
	unsigned int l, i;

	sprintf(buffer, level == RAW ? "%s" : "[%s] ",
		level < 0 || level > (sizeof(log_level_str) / sizeof(log_level_str[0]))
		? "?" : log_level_str[level]);

	l = strnlen(buffer, PRINTK_BUFFER);
	va_start(args, fmt);
	l = vsprintf(&buffer[l], fmt, args);
	va_end(args);
    
	for (i = 0; i < num_handlers; i++) {
		printk_handlers[i](buffer);
	}
}
