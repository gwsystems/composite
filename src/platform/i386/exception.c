#include <pgtbl.h>
#include <thd.h>

#include "kernel.h"
#include "string.h"
#include "isr.h"
#include "chal_cpu.h"

#define PRINTK(format, ...) printk("(CPU%ld:) " format, get_cpuid(), ## __VA_ARGS__)

void
print_regs_state(struct pt_regs *regs)
{
	PRINTK("registers:\n");
	PRINTK("General registers-> EAX: %x, EBX: %x, ECX: %x, EDX: %x\n", regs->ax, regs->bx, regs->cx, regs->dx);
	PRINTK("Segment registers-> CS: %x, DS: %x, ES: %x, FS: %x, GS: %x, SS: %x\n", regs->cs, regs->ds, regs->es,
	       regs->fs, regs->gs, regs->ss);
	PRINTK("Index registers-> ESI: %x, EDI: %x, EIP: %x, ESP: %x, EBP: %x\n", regs->si, regs->di, regs->ip,
	       regs->sp, regs->bp);
	PRINTK("Indicator-> EFLAGS: %x\n", regs->flags);
	PRINTK("(Exception Error Code-> ORIG_AX: %x)\n", regs->orig_ax);
}

void
fault_regs_save(struct pt_regs *regs, struct thread *thd)
{
	copy_all_regs(regs, &(thd->fault_regs));
}

void
fault_handler_sinv(struct pt_regs *regs, capid_t cap)
{
	struct cos_cpu_local_info *ci       = cos_cpu_local_info();
	struct thread *            curr_thd = thd_current(ci);
	struct cap_header *        fh;
	struct comp_info *         cos_info;
	struct comp_invstk_info *  cos_invstk_info;
	thdid_t                    thdid = curr_thd->tid;
	unsigned long              ip, sp;
	u32_t                      fault_addr = 0, errcode, eip;

	fault_regs_save (regs, curr_thd);
	cos_invstk_info = thd_invstk_current(curr_thd, &ip, &sp, ci);
	cos_info = thd_invstk_comp_info_get(cos_invstk_info);
	fh = captbl_lkup(cos_info->captbl, cap);
	__userregs_sinvargset(regs, regs->sp, regs->ip, fault_addr, cap);

	if (unlikely(!fh)) {
		die("FAULT: Fault handler not found\n");
		return;
	}

	sinv_call(curr_thd, (struct cap_sinv *)fh, regs, ci, 1);
}

int
div_by_zero_err_fault_handler(struct pt_regs *regs)
{
	fault_handler_sinv(regs, FAULT_CAPTBL_DIVZERO);
	return 1;
}

int
debug_trap_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("TRAP: Debug\n");

	return 1;
}

int
breakpoint_trap_handler(struct pt_regs *regs)
{
	fault_handler_sinv(regs, FAULT_CAPTBL_BRKPT);
	return 1;
}

int
overflow_trap_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("TRAP: Overflow\n");

	return 1;
}

int
bound_range_exceed_fault_handler(struct pt_regs *regs)
{
	fault_handler_sinv(regs, FAULT_CAPTBL_INVSTK);
	return 1;
}

int
invalid_opcode_fault_handler(struct pt_regs *regs)
{
	fault_handler_sinv(regs, FAULT_CAPTBL_INVLD_INS);
	return 1;
}

int
device_not_avail_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: Device Not Available\n");

	return 1;
}

int
double_fault_abort_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("ABORT: Double Fault\n");

	return 1;
}

int
invalid_tss_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: Invalid TSS\n");

	return 1;
}

int
seg_not_present_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: Segment Not Present\n");

	return 1;
}

int
stack_seg_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: Stack Segment Fault\n");

	return 1;
}

int
gen_protect_fault_handler(struct pt_regs *regs)
{
	fault_handler_sinv(regs, FAULT_CAPTBL_MEM_ACCESS);
	return 1;
}

int
page_fault_handler(struct pt_regs *regs)
{
	fault_handler_sinv(regs, FAULT_CAPTBL_MEM_ACCESS);
	return 1;
}

int
x87_float_pt_except_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: x87 Floating-point Exception\n");

	return 1;
}

int
align_check_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: Alignment Check\n");

	return 1;
}

int
machine_check_abort_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("ABORT: Machine Check\n");

	return 1;
}

int
smid_float_pt_except_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: SMID Floating-point Exception\n");

	return 1;
}

int
virtualization_except_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: Virtualization Exception\n");

	return 1;
}

int
security_except_fault_handler(struct pt_regs *regs)
{
	print_regs_state(regs);
	die("FAULT: Security Exception\n");

	return 1;
}
