#include "shared/cos_types.h"
#include "string.h"
#include "idt.h"
#include "isr.h"
#include "printk.h"
#include "ports.h"

/* Information taken from: http://wiki.osdev.org/PIC */
/* FIXME:  Remove magic numbers and replace with this */
#define PIC1            0x20
#define PIC2            0xA0
#define PIC1_COMMAND    PIC1
#define PIC1_DATA       (PIC1 + 1)
#define PIC2_COMMAND    PIC2
#define PIC2_DATA       (PIC2 + 1)

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8 and 70, as configured by default */
 
#define ICW1_ICW4       0x01        /* ICW4 (not) needed */
#define ICW1_SINGLE     0x02        /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04        /* Call address interval 4 (8) */
#define ICW1_LEVEL      0x08        /* Level triggered (edge) mode */
#define ICW1_INIT       0x10        /* Initialization - required! */
 
#define ICW4_8086       0x01        /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02        /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08        /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C        /* Buffered mode/master */
#define ICW4_SFNM       0x10        /* Special fully nested (not) */
#define ICW1_ICW4       0x01 



struct idt_entry {
    u16_t base_lo;   // Lower 16 bits of address to jump too after int
    u16_t sel;       // Kernel segment selector
    u8_t zero;       // Must always be zero
    u8_t flags;      // flags
    u16_t base_hi;   // Upper 16 bits of addres to jump too
} __attribute__((packed));

struct idt_ptr {
    u16_t limit;
    u32_t base;     // Addres of first element
} __attribute__((packed));

// Always must be 256
#define NUM_IDT_ENTRIES 256

extern void idt_flush(u32_t);

struct idt_entry idt_entries[NUM_IDT_ENTRIES];
struct idt_ptr idt_ptr;

static void
idt_set_gate(u8_t num, u32_t base, u16_t sel, u8_t flags)
{
    idt_entries[num].base_lo = base & 0xFFFF;
    idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

    idt_entries[num].sel  = sel;
    idt_entries[num].zero = 0;

    /* FIXME: This does not yet allow for mode switching */
    idt_entries[num].flags = flags /* | 0x60 */; 
    // The OR is used for ring once we get usermode up and running
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
idt__init(void)
{
    idt_ptr.limit = (sizeof(struct idt_entry) * NUM_IDT_ENTRIES) - 1;
    idt_ptr.base  = (u32_t)&idt_entries;

    memset(&idt_entries, 0, sizeof(struct idt_entry) * NUM_IDT_ENTRIES);

    // Remap the irq table.
    // FIXME: Move to the remap_irq_table function
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

    /* FIXME: Replace 0 with ISR0, etc */
    idt_set_gate(0, (u32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (u32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (u32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (u32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (u32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (u32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (u32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (u32_t)isr7, 0x08, 0x8E);
    //idt_set_gate(8, (u32_t)isr8, 0x08, 0x8E);		// disabled to make APM halt work
    idt_set_gate(9, (u32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (u32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (u32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (u32_t)isr12, 0x08, 0x8E);
    //idt_set_gate(13, (u32_t)isr13, 0x08, 0x8E);		// disabled to make APM halt work
    idt_set_gate(14, (u32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (u32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (u32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (u32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (u32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (u32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (u32_t)isr20, 0x08, 0x8E);
    //idt_set_gate(21, (u32_t)isr21, 0x08, 0x8E);		// disabled to make APM halt work
    idt_set_gate(22, (u32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (u32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (u32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (u32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (u32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (u32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (u32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (u32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (u32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (u32_t)isr31, 0x08, 0x8E);

    /* Setup IRQS */
    idt_set_gate(IRQ0, (u32_t)irq0, 0x08, 0x8E);
    idt_set_gate(IRQ1, (u32_t)irq1, 0x08, 0x8E);
    idt_set_gate(IRQ2, (u32_t)irq2, 0x08, 0x8E);
    idt_set_gate(IRQ3, (u32_t)irq3, 0x08, 0x8E);
    idt_set_gate(IRQ4, (u32_t)irq4, 0x08, 0x8E);
    idt_set_gate(IRQ5, (u32_t)irq5, 0x08, 0x8E);
    idt_set_gate(IRQ6, (u32_t)irq6, 0x08, 0x8E);
    idt_set_gate(IRQ7, (u32_t)irq7, 0x08, 0x8E);
    idt_set_gate(IRQ8, (u32_t)irq8, 0x08, 0x8E);
    idt_set_gate(IRQ9, (u32_t)irq9, 0x08, 0x8E);
    idt_set_gate(IRQ10, (u32_t)irq10, 0x08, 0x8E);
    idt_set_gate(IRQ11, (u32_t)irq11, 0x08, 0x8E);
    idt_set_gate(IRQ12, (u32_t)irq12, 0x08, 0x8E);
    idt_set_gate(IRQ13, (u32_t)irq13, 0x08, 0x8E);
    idt_set_gate(IRQ14, (u32_t)irq14, 0x08, 0x8E);
    idt_set_gate(IRQ15, (u32_t)irq15, 0x08, 0x8E);

    /* Seems better to do it inline here */
    struct {
        unsigned short length;
        unsigned long base;
    } __attribute__((__packed__)) IDTR;

    IDTR.length = idt_ptr.limit;
    IDTR.base = (unsigned long)idt_entries;

    //printk(INFO, "base: %ld\n", IDTR.base);

    asm volatile("lidt (%0)" : : "p"(&IDTR));

    //idt_flush((u32_t)&idt_ptr);
}

