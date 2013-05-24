#include "types.h"
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
    uint16_t base_lo;   // Lower 16 bits of address to jump too after int
    uint16_t sel;       // Kernel segment selector
    uint8_t zero;       // Must always be zero
    uint8_t flags;      // flags
    uint16_t base_hi;   // Upper 16 bits of addres to jump too
} __attribute__((packed));

/* FIXME: Look at intel spec should this be a uintptr_t or uint32_t ?? */
struct idt_ptr {
    uint16_t limit;
    uintptr_t base;     // Addres of first element
} __attribute__((packed));

// Always must be 256
#define NUM_IDT_ENTRIES 256

extern void idt_flush(uintptr_t);

struct idt_entry idt_entries[NUM_IDT_ENTRIES];
struct idt_ptr idt_ptr;

static void
idt_set_gate(uint8_t num, uintptr_t base, uint16_t sel, uint8_t flags)
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
    uint8_t pic1_mask;
    uint8_t pic2_mask;

    // Save masks
    pic1_mask = inb(PIC1_DATA); 
    pic2_mask = inb(PIC2_DATA);
}
#endif

void 
idt__init(void)
{
    idt_ptr.limit = (sizeof(struct idt_entry) * NUM_IDT_ENTRIES) - 1;
    idt_ptr.base  = (uintptr_t)&idt_entries;

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
    idt_set_gate(0, (uintptr_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uintptr_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uintptr_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uintptr_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uintptr_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uintptr_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uintptr_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uintptr_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uintptr_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uintptr_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uintptr_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uintptr_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uintptr_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uintptr_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uintptr_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uintptr_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uintptr_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uintptr_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uintptr_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uintptr_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uintptr_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uintptr_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uintptr_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uintptr_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uintptr_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uintptr_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uintptr_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uintptr_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uintptr_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uintptr_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uintptr_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uintptr_t)isr31, 0x08, 0x8E);

    /* Setup IRQS */
    idt_set_gate(IRQ0, (uintptr_t)irq0, 0x08, 0x8E);
    idt_set_gate(IRQ1, (uintptr_t)irq1, 0x08, 0x8E);
    idt_set_gate(IRQ2, (uintptr_t)irq2, 0x08, 0x8E);
    idt_set_gate(IRQ3, (uintptr_t)irq3, 0x08, 0x8E);
    idt_set_gate(IRQ4, (uintptr_t)irq4, 0x08, 0x8E);
    idt_set_gate(IRQ5, (uintptr_t)irq5, 0x08, 0x8E);
    idt_set_gate(IRQ6, (uintptr_t)irq6, 0x08, 0x8E);
    idt_set_gate(IRQ7, (uintptr_t)irq7, 0x08, 0x8E);
    idt_set_gate(IRQ8, (uintptr_t)irq8, 0x08, 0x8E);
    idt_set_gate(IRQ9, (uintptr_t)irq9, 0x08, 0x8E);
    idt_set_gate(IRQ10, (uintptr_t)irq10, 0x08, 0x8E);
    idt_set_gate(IRQ11, (uintptr_t)irq11, 0x08, 0x8E);
    idt_set_gate(IRQ12, (uintptr_t)irq12, 0x08, 0x8E);
    idt_set_gate(IRQ13, (uintptr_t)irq13, 0x08, 0x8E);
    idt_set_gate(IRQ14, (uintptr_t)irq14, 0x08, 0x8E);
    idt_set_gate(IRQ15, (uintptr_t)irq15, 0x08, 0x8E);

    /* Seems better to do it inline here */
    struct {
        unsigned short length;
        unsigned long base;
    } __attribute__((__packed__)) IDTR;

    IDTR.length = idt_ptr.limit;
    IDTR.base = (unsigned long)idt_entries;

    //printk(INFO, "base: %ld\n", IDTR.base);

    asm volatile("lidt (%0)" : : "p"(&IDTR));

    //idt_flush((uintptr_t)&idt_ptr);
}

