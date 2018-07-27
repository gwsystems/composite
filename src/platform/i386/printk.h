#ifndef PRINTK_H
#define PRINTK_H

typedef enum {
	PRINTK_SERIAL = 0,
	PRINTK_VGA,
} printk_t;

#define PRINTK_MAX_HANDLERS 2

int printk_register_handler(printk_t, void (*handler)(const char *));

#endif /* PRINTK_H */
