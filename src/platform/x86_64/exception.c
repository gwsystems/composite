#include <pgtbl.h>
#include <thread.h>
#include <fpu.h>
#include <kernel.h>
#include <string.h>
#include <isr.h>
#include <chal_cpu.h>
#include <chal_regs.h>
#include <chal_state.h>
#include <cos_compiler.h>

static void
trap_error_check(const char *name, struct regs *r)
{
	/* Is the trap not coming from user-level? Bomb out. */
	if ((r->frame.cs & 3) != 3) die_reg(r, "KERNEL FAULT %s\n", name);
}

#define TRAP_C_HANDLER(name, fn)		\
void name(struct regs *r)			\
{						\
	/*trap_error_check(EXPAND(name), r);*/	\
	r = fn(r);				\
	userlevel_eager_return(r);		\
}

#define TRAP_C_ERR_HANDLER(name, msg)			\
void							\
name(struct regs *rs)					\
{							\
	rs = error_handler(rs, msg);			\
	userlevel_eager_return(rs);			\
}

static struct regs *
error_handler(struct regs *rs, const char *err_msg)
{
	panic(err_msg, rs);

	return rs;
}

TRAP_C_ERR_HANDLER(div_by_zero_err_fault_handler, "FAULT: Divide by Zero\n");
TRAP_C_ERR_HANDLER(debug_trap_handler, "TRAP: Debug\n");
TRAP_C_ERR_HANDLER(breakpoint_trap_handler, "TRAP: Breakpoint\n");
TRAP_C_ERR_HANDLER(overflow_trap_handler, "TRAP: Overflow\n");
TRAP_C_ERR_HANDLER(bound_range_exceed_fault_handler, "FAULT: Bound Range Exceeded\n");
TRAP_C_ERR_HANDLER(invalid_opcode_fault_handler, "FAULT: Invalid opcode\n");
TRAP_C_ERR_HANDLER(device_not_avail_fault_handler, "FAULT: Device not available\n");
TRAP_C_ERR_HANDLER(double_fault_abort_handler, "ABORT: Double Fault\n");
TRAP_C_ERR_HANDLER(invalid_tss_fault_handler, "FAULT: Invalid TSS\n");
TRAP_C_ERR_HANDLER(seg_not_present_fault_handler, "FAULT: Segment Not Present\n");
TRAP_C_ERR_HANDLER(stack_seg_fault_handler, "FAULT: Stack Segment Fault\n");
TRAP_C_ERR_HANDLER(gen_protect_fault_handler, "FAULT: General Protection Fault\n");
TRAP_C_ERR_HANDLER(x87_float_pt_except_fault_handler, "FAULT: x87 Floating-point Exception\n");
TRAP_C_ERR_HANDLER(align_check_fault_handler, "FAULT: Alignment Check\n");
TRAP_C_ERR_HANDLER(machine_check_abort_handler, "ABORT: Machine Check\n");
TRAP_C_ERR_HANDLER(simd_float_pt_except_fault_handler, "FAULT: SMID Floating-point Exception\n");
TRAP_C_ERR_HANDLER(virtualization_except_fault_handler, "FAULT: Virtualization Exception\n");
TRAP_C_ERR_HANDLER(security_except_fault_handler, "FAULT: Security Exception\n");

struct regs *serial_fn(struct regs *r);
TRAP_C_HANDLER(serial_handler, serial_fn);
struct regs *lapic_spurious_fn(struct regs *regs);
struct regs *lapic_ipi_fn(struct regs *regs);
struct regs *lapic_timer_fn(struct regs *regs);
TRAP_C_HANDLER(lapic_spurious_handler, lapic_spurious_fn);
TRAP_C_HANDLER(lapic_ipi_handler, lapic_ipi_fn);
TRAP_C_HANDLER(lapic_timer_handler, lapic_timer_fn);

struct regs *
page_fault_fn(struct regs *regs)
{
	unsigned long fault_addr = 0, errcode = 0, ip = 0;
	struct state *ci    = state();
	struct thread *curr = ci->active_thread;
	thdid_t       thdid = curr->id;

	fault_addr = chal_cpu_fault_vaddr(regs);
	errcode    = chal_cpu_fault_errcode(regs);
	ip         = chal_cpu_fault_ip(regs);

	die("FAULT: Page Fault in thd %d (%s %s %s %s %s) @ 0x%p, ip 0x%p, tls 0x%p\n", thdid,
	    errcode & X86_PGTBL_PRESENT ? "present" : "not-present",
	    errcode & X86_PGTBL_WRITABLE ? "write-fault" : "read-fault", errcode & X86_PGTBL_USER ? "user-mode" : "kernel-mode",
	    errcode & X86_PGTBL_WT ? "reserved" : "", errcode & X86_PGTBL_NOCACHE ? "instruction-fetch" : "", fault_addr, ip, curr->tls);

	return regs;
}
TRAP_C_HANDLER(page_fault_handler, page_fault_fn);

struct regs *
oneshot_timer_fn(struct regs *regs)
{
	printk(COS_REGS_PRINT_ARGS(regs));
	die("FAULT: Oneshot Timer Exception\n");

	return regs;
}
TRAP_C_HANDLER(oneshot_handler, oneshot_timer_fn);

struct regs *
periodic_timer_fn(struct regs *regs)
{
	printk(COS_REGS_PRINT_ARGS(regs));
	die("FAULT: Periodic Timer Exception\n");

	return regs;
}
TRAP_C_HANDLER(periodic_handler, periodic_timer_fn);
