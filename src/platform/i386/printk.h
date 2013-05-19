#ifndef _PRINTK_H_
#define _PRINTK_H_

enum log_level {
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

void printk__init(void);
void printk(enum log_level level, const char *fmt, ...);
int printk__register_handler(void (*handler)(const char *));

#define die(fmt,...) do {               \
    printk(ERROR, fmt,##__VA_ARGS__);   \
    asm volatile ("hlt");               \
} while(0)

#endif

