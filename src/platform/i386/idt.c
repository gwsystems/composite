#include "kernel.h"
#include "string.h"
#include "isr.h"
#include "chal/io.h"

/* Information taken from: http://wiki.osdev.org/PIC */
/* FIXME:  Remove magic numbers and replace with this */
#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8 and 70, as configured by default */

#define ICW1_ICW4 0x01      /* ICW4 (not) needed */
#define ICW1_SINGLE 0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08     /* Level triggered (edge) mode */
#define ICW1_INIT 0x10      /* Initialization - required! */

#define ICW4_8086 0x01       /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02       /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM 0x10       /* Special fully nested (not) */
#define ICW1_ICW4 0x01

struct idt_entry {
	u16_t base_lo; // Lower 16 bits of address to jump too after int
	u16_t sel;     // Kernel segment selector
	u8_t  zero;    // Must always be zero
	u8_t  flags;   // flags
	u16_t base_hi; // Upper 16 bits of addres to jump too
} __attribute__((packed));

struct idt_ptr {
	u16_t limit;
	u32_t base; // Addres of first element
} __attribute__((packed));

// Always must be 256
#define NUM_IDT_ENTRIES 256

extern void idt_flush(u32_t);

struct idt_entry idt_entries[NUM_IDT_ENTRIES];
struct idt_ptr   idt_ptr;

static void
idt_set_gate(u8_t num, u32_t base, u16_t sel, u8_t flags)
{
	int cpu_id = get_cpuid();
	idt_entries[num].base_lo = base & 0xFFFF;
	idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

	idt_entries[num].sel  = sel;
	idt_entries[num].zero = 0;

	/* FIXME: This does not yet allow for mode switching */
	idt_entries[num].flags = flags /* | 0x60 */;
	// The OR is used for ring once we get usermode up and running
}

int
hw_handler(struct pt_regs *regs)
{
	int preempt = 1;

	/*
	 * TODO: ack here? or
	 *       after user-level interrupt(rcv event) processing?
	 */
	ack_irq(regs->orig_ax);
	preempt = cap_hw_asnd(&hw_asnd_caps[regs->orig_ax], regs);

	return preempt;
}

#if 0
static inline void
remap_irq_table(void)
{
	u8_t pic1_mask;
	u8_t pic2_mask;

	// Save masks
	pic1_mask = inb(PIC1_DATA);
	pic2_mask = inb(PIC2_DATA);
}
#endif

void
idt_init(const cpuid_t cpu_id)
{
	idt_ptr.limit = (sizeof(struct idt_entry) * NUM_IDT_ENTRIES) - 1;
	idt_ptr.base  = (u32_t)&(idt_entries);
	memset(&(idt_entries), 0, sizeof(struct idt_entry) * NUM_IDT_ENTRIES);

	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);

	idt_set_gate(IRQ_DIV_BY_ZERO_ERR_FAULT, (u32_t)div_by_zero_err_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_DEBUG_TRAP, (u32_t)debug_trap_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_BREAKPOINT_TRAP, (u32_t)breakpoint_trap_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_OVERFLOW_TRAP, (u32_t)overflow_trap_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_BOUND_RANGE_EXCEED_FAULT, (u32_t)bound_range_exceed_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_INVALID_OPCODE_FAULT, (u32_t)invalid_opcode_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_DEVICE_NOT_AVAIL_FAULT, (u32_t)device_not_avail_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_DOUBLE_FAULT_ABORT, (u32_t)double_fault_abort_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_INVALID_TSS_FAULT, (u32_t)invalid_tss_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_SEG_NOT_PRESENT_FAULT, (u32_t)seg_not_present_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_STACK_SEG_FAULT, (u32_t)stack_seg_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_GEN_PROTECT_FAULT, (u32_t)gen_protect_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_PAGE_FAULT, (u32_t)page_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_X87_FLOAT_PT_EXCEPT_FAULT, (u32_t)x87_float_pt_except_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_ALIGN_CHECK_FAULT, (u32_t)align_check_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_MACHINE_CHECK_ABORT, (u32_t)machine_check_abort_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_SMID_FLOAT_PT_EXCEPT_FAULT, (u32_t)smid_float_pt_except_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_VIRTUALIZATION_EXCEPT_FAULT, (u32_t)virtualization_except_fault_irq, 0x08, 0x8E);
	idt_set_gate(IRQ_SECURITY_EXCEPT_FAULT, (u32_t)security_except_fault_irq, 0x08, 0x8E);

	idt_set_gate(HW_PERIODIC, (u32_t)periodic_irq, 0x08, 0x8E);
	idt_set_gate(HW_KEYBOARD, (u32_t)keyboard_irq, 0x08, 0x8E);
	idt_set_gate(HW_ID3, (u32_t)handler_hw_34, 0x08, 0x8E);
	idt_set_gate(HW_ID4, (u32_t)handler_hw_35, 0x08, 0x8E);
	idt_set_gate(HW_SERIAL, (u32_t)serial_irq, 0x08, 0x8E);
	idt_set_gate(HW_ID6, (u32_t)handler_hw_37, 0x08, 0x8E);
	idt_set_gate(HW_ID7, (u32_t)handler_hw_38, 0x08, 0x8E);
	idt_set_gate(HW_ID8, (u32_t)handler_hw_39, 0x08, 0x8E);
	idt_set_gate(HW_ONESHOT, (u32_t)oneshot_irq, 0x08, 0x8E);
	idt_set_gate(HW_ID10, (u32_t)handler_hw_41, 0x08, 0x8E);
	idt_set_gate(HW_ID11, (u32_t)handler_hw_42, 0x08, 0x8E);
	idt_set_gate(HW_ID12, (u32_t)handler_hw_43, 0x08, 0x8E);
	idt_set_gate(HW_ID13, (u32_t)handler_hw_44, 0x08, 0x8E);
	idt_set_gate(HW_ID14, (u32_t)handler_hw_45, 0x08, 0x8E);
	idt_set_gate(HW_ID15, (u32_t)handler_hw_46, 0x08, 0x8E);
	idt_set_gate(HW_ID16, (u32_t)handler_hw_47, 0x08, 0x8E);
	idt_set_gate(HW_ID17, (u32_t)handler_hw_48, 0x08, 0x8E);
	idt_set_gate(HW_ID18, (u32_t)handler_hw_49, 0x08, 0x8E);
	idt_set_gate(HW_ID19, (u32_t)handler_hw_50, 0x08, 0x8E);
	idt_set_gate(HW_ID20, (u32_t)handler_hw_51, 0x08, 0x8E);
	idt_set_gate(HW_ID21, (u32_t)handler_hw_52, 0x08, 0x8E);
	idt_set_gate(HW_ID22, (u32_t)handler_hw_53, 0x08, 0x8E);
	idt_set_gate(HW_ID23, (u32_t)handler_hw_54, 0x08, 0x8E);
	idt_set_gate(HW_ID24, (u32_t)handler_hw_55, 0x08, 0x8E);
	idt_set_gate(HW_ID25, (u32_t)handler_hw_56, 0x08, 0x8E);
	idt_set_gate(HW_ID26, (u32_t)handler_hw_57, 0x08, 0x8E);
	idt_set_gate(HW_ID27, (u32_t)handler_hw_58, 0x08, 0x8E);
	idt_set_gate(HW_ID28, (u32_t)handler_hw_59, 0x08, 0x8E);
	idt_set_gate(HW_ID29, (u32_t)handler_hw_60, 0x08, 0x8E);
	idt_set_gate(HW_ID30, (u32_t)handler_hw_61, 0x08, 0x8E);
	idt_set_gate(HW_ID31, (u32_t)handler_hw_62, 0x08, 0x8E);
	idt_set_gate(HW_LAPIC_SPURIOUS, (u32_t)lapic_spurious_irq, 0x08, 0x8E);
	idt_set_gate(HW_LAPIC_IPI_ASND, (u32_t)lapic_ipi_asnd_irq, 0x08, 0x8E);
	idt_set_gate(HW_LAPIC_TIMER, (u32_t)lapic_timer_irq, 0x08, 0x8E);

	struct {
		unsigned short length;
		unsigned long  base;
	} __attribute__((__packed__)) idtr;

	idtr.length = idt_ptr.limit;
	idtr.base   = (unsigned long)(&(idt_entries));

	/* asm volatile("lidt (%0)" : : "p"(&idtr)); */
	idt_flush((u32_t)&idtr);
}
