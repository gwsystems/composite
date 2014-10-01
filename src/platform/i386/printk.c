#include "string.h"
#include "ports.h"
#include "shared/cos_types.h"
#include "vga.h"
#include "printk.h"

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

static const char *
log_level_to_string(enum log_level level)
{
    if (level >= 0 && level < (sizeof(log_level_str) / sizeof(log_level_str[0])))
        return log_level_str[level];
    else
        return "UNKOWN";
}

int
printk__register_handler(void (*handler)(const char *))
{
    if (handler == NULL)
        return -1;
   
    if (num_handlers > (sizeof(printk_handlers) / sizeof(printk_handlers[0])))
        return -1;

    printk_handlers[num_handlers++] = handler;

    return 0;
}

void 
printk(enum log_level level, const char *fmt, ...)
{
    char buffer[PRINTK_BUFFER];
	va_list args;
    int l;
    unsigned i;

    sprintf(buffer, level == RAW ? "%s" : "[%s] ", log_level_to_string(level));
    l = strlen(buffer);

    va_start(args, fmt);
	l = vsprintf(&buffer[l], fmt, args);
	va_end(args);

    vga__puts(buffer);
    
    for (i = 0; i < num_handlers; i++)
        printk_handlers[i](buffer);
}

void
printk__init(void) 
{
    vga__clear();
}
       
