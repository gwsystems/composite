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
print_pt_regs(struct pt_regs* regs)
{
	PRINTK("Register hexdump (0x%p):\n", regs);
	PRINTK("Argument-> R0: 0x%x, R1: 0x%x, R2: 0x%x, R3: 0x%x\n",
	       regs->r0, regs->r1, regs->r2, regs->r3);
	PRINTK("Variable-> R4: 0x%x, R5: 0x%x, R6: 0x%x, R7: 0x%x\n",
	       regs->r4, regs->r5, regs->r6, regs->r7);
	PRINTK("           R8: 0x%x, R9: 0x%x, R10: 0x%x, R11: 0x%x\n",
	       regs->r8, regs->r9, regs->r10, regs->r11);
	PRINTK("Special->  R12 (IP): 0x%x, R13 (SP): 0x%x, R14 (LR): 0x%x, R15 (PC): 0x%x\n",
	       regs->r12, regs->r13_sp, regs->r14_lr, regs->r15_pc);
	PRINTK("(CPSR: 0x%x)\n", regs->cpsr);
}

void
dbgprint(void)
{
	PRINTK("Debug print string active\n");
}

void
undefined_dbgprint(struct pt_regs *regs)
{
	PRINTK("Undefined handler!!\n");
	print_pt_regs(regs);
}

void
prefetch_abort_dbgprint(struct pt_regs *regs)
{
	PRINTK("Prefetch Abort handler!!\n");
	print_pt_regs(regs);
	PRINTK("IFSR: 0x%lx, IFAR: 0x%lx\n", __cos_cav7_ifsr_get(), __cos_cav7_ifar_get());
}

void
data_abort_dbgprint(struct pt_regs *regs)
{
	PRINTK("Data Abort handler!!\n");
	print_pt_regs(regs);
	PRINTK("DFSR: 0x%lx, DFAR: 0x%lx\n", __cos_cav7_dfsr_get(), __cos_cav7_dfar_get());
	PRINTK("ADFSR: 0x%lx\n", __cos_cav7_adfsr_get());
}

void
fiq_dbgprint(void)
{
	PRINTK("FIQ handler!!\n");
}

void
dbgval(unsigned long val)
{
	PRINTK("Debug value 0x%x\n", val);
}
