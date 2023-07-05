#include "chal_regs.h"
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

	for (i = 0; i < num_handlers; i++) {
		printk_handlers[i](buffer);
	}
}

void
print_pt_regs(struct regs *regs)
{
	u64_t sp, bp, ip, cx, r11, flags;

	if (regs->state == REG_STATE_SYSCALL) {
		sp = regs->clobbered.rbp_sp;
		bp = 0;
		cx = 0;
		ip = regs->clobbered.rcx_ip;
		flags = regs->clobbered.r11;
		r11 = 0;
	} else {
		sp = regs->frame.sp;
		ip = regs->frame.ip;
		cx = regs->clobbered.rcx_ip;
		flags = regs->frame.flags;
		bp = regs->clobbered.rbp_sp;
		r11 = regs->clobbered.r11;
	}

	PRINTK("Register hexdump (0x%p):\n", regs);
	PRINTK("General->   RAX: 0x%p, RBX: 0x%p, RCX: 0x%p, RDX: 0x%p\n",
		regs->args[0], regs->args[1], cx, regs->args[2]);
	PRINTK("General->   R8: 0x%p, R9: 0x%p, R10: 0x%p, R11: 0x%p\n",
		regs->args[5], regs->args[6], regs->args[7], r11);
	PRINTK("General->   R12: 0x%p, R13: 0x%p, R14: 0x%p, R15: 0x%p\n",
		regs->args[8], regs->args[9], regs->args[10], regs->args[11]);
	PRINTK("Segment->   CS: 0x%x, SS: 0x%x\n",
		regs->frame.cs, regs->frame.ss);
	PRINTK("Index->     RSI: 0x%p, RDI: 0x%p, RIP: 0x%p, RSP: 0x%p, RBP: 0x%p\n",
		regs->args[3], regs->args[4], ip, sp, bp);
	PRINTK("Indicator-> RFLAGS: 0x%p\n", flags);
	if (regs->state == REG_STATE_PREEMPTED) {
		PRINTK("(Exception Error Code: 0x%x)\n", regs->frame.errcode);
	}
}
