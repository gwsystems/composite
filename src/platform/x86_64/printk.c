#include <cos_regs.h>
#include "string.h"
#include "kernel.h"

#define PRINTK_BUFFER 1024

void
printk(const char *fmt, ...)
{
	char         buffer[PRINTK_BUFFER];
	va_list      args;

	va_start(args, fmt);
	vsprintf(buffer, fmt, args);
	va_end(args);
	puts(buffer);
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
