#include <pgtbl.h>
#include <thread.h>
#include <fpu.h>
#include <kernel.h>
#include <string.h>
#include <isr.h>
#include <chal_cpu.h>

int
div_by_zero_err_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Divide by Zero Error\n\n");

	return 1;
}

int
debug_trap_handler(struct regs *regs)
{
	print_regs(regs);
	die("TRAP: Debug\n");

	return 1;
}

int
breakpoint_trap_handler(struct regs *regs)
{
	print_regs(regs);
	die("TRAP: Breakpoint\n");

	return 1;
}

int
overflow_trap_handler(struct regs *regs)
{
	print_regs(regs);
	die("TRAP: Overflow\n");

	return 1;
}

int
bound_range_exceed_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Bound Range Exceeded\n");

	return 1;
}

int
invalid_opcode_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Invalid opcode\n");

	return 1;
}

int
device_not_avail_fault_handler(struct regs *regs)
{
	int ret = fpu_disabled_exception_handler();

	if (!ret) {
		print_regs(regs);
		die("FAULT: Device not available\n");
	}

	return ret;
}

int
double_fault_abort_handler(struct regs *regs)
{
	print_regs(regs);
	die("ABORT: Double Fault\n");

	return 1;
}

int
invalid_tss_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Invalid TSS\n");

	return 1;
}

int
seg_not_present_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Segment Not Present\n");

	return 1;
}

int
stack_seg_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Stack Segment Fault\n");

	return 1;
}

int
gen_protect_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: General Protection Fault\n");
	return 1;
}

int
page_fault_handler(struct regs *regs)
{
	unsigned long fault_addr = 0, errcode = 0, ip = 0;
	struct state *ci    = state();
	struct thread *curr = ci->active_thread;
	thdid_t       thdid = curr->id;

	print_regs(regs);
	fault_addr = chal_cpu_fault_vaddr(regs);
	errcode    = chal_cpu_fault_errcode(regs);
	ip         = chal_cpu_fault_ip(regs);

	die("FAULT: Page Fault in thd %d (%s %s %s %s %s) @ 0x%p, ip 0x%p, tls 0x%p\n", thdid,
	    errcode & X86_PGTBL_PRESENT ? "present" : "not-present",
	    errcode & X86_PGTBL_WRITABLE ? "write-fault" : "read-fault", errcode & X86_PGTBL_USER ? "user-mode" : "system",
	    errcode & X86_PGTBL_WT ? "reserved" : "", errcode & X86_PGTBL_NOCACHE ? "instruction-fetch" : "", fault_addr, ip, curr->tls);

	return 1;
}

int
x87_float_pt_except_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: x87 Floating-point Exception\n");

	return 1;
}

int
align_check_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Alignment Check\n");

	return 1;
}

int
machine_check_abort_handler(struct regs *regs)
{
	print_regs(regs);
	die("ABORT: Machine Check\n");

	return 1;
}

int
simd_float_pt_except_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: SMID Floating-point Exception\n");

	return 1;
}

int
virtualization_except_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Virtualization Exception\n");

	return 1;
}

int
security_except_fault_handler(struct regs *regs)
{
	print_regs(regs);
	die("FAULT: Security Exception\n");

	return 1;
}
