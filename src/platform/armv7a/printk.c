#include "string.h"
#include "kernel.h"

#define PRINTK_BUFFER 1024
#define MAX_HANDLERS 5

static void (*printk_handlers[MAX_HANDLERS])(const char *);
static unsigned num_handlers = 0;

int
printk_register_handler(void (*handler)(const char *))
{
	if (handler == NULL || num_handlers > (sizeof(printk_handlers) / sizeof(printk_handlers[0]))) return -1;
	printk_handlers[num_handlers++] = handler;
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

	for (i = 0; i < num_handlers; i++) { printk_handlers[i](buffer); }
}

void
print_arch_regs(void)
{
	printk("DFSR: %lx, DFAR: 0x%lx\n", __cos_cav7_dfsr_get(), __cos_cav7_dfar_get());
	printk("IFSR: %lx, IFAR: 0x%lx\n", __cos_cav7_ifsr_get(), __cos_cav7_ifar_get());
	printk("ADFSR: %lx\n", __cos_cav7_adfsr_get());
}

void
dbgprint(void)
{
	printk("Debug print string active\n");
}

void
undefined_dbgprint(struct pt_regs *regs)
{
	printk("Undefined handler!!\n");
	print_arch_regs();
	regs_print(regs);
}

void
prefetch_abort_dbgprint(struct pt_regs *regs)
{
	printk("Prefetch Abort handler!!\n");
	print_arch_regs();
	regs_print(regs);
}

void
data_abort_dbgprint(struct pt_regs *regs)
{
	printk("Data Abort handler!!\n");
	print_arch_regs();
	regs_print(regs);
}

void
fiq_dbgprint(void)
{
	printk("FIQ handler!!\n");
	print_arch_regs();
}

void
dbgval(unsigned long val)
{
	printk("Debug value %x\n", val);
}
