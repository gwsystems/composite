#ifndef _PRINTK_H_
#define _PRINTK_H_

enum log_level {
    RAW,
    DEBUG,
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
    asm("mov $0x53,%ah");               \
    asm("mov $0x07,%al");               \
    asm("mov $0x001,%bx");              \
    asm("mov $0x03,%cx");               \
    asm("int $0x15");                   \
} while(0)

#endif

