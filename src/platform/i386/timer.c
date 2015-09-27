#define ENABLE_TIMER

#include <thd.h>
#include <inv.h>

#include "isr.h"
#include "io.h"
#include "kernel.h"

#define PIT_A       0x40
#define PIT_B       0x41
#define PIT_C       0x42
#define PIT_CONTROL 0x43
#define PIT_SET     0x36
#define PIT_MASK    0xFF
#define PIT_SCALE   1193180

/* FIXME: per-thread */
static struct thread *timer_thread = NULL;

void
timer_handler(struct pt_regs *rs)
{
	ack_irq(IRQ_PIT);
	printk("t");
//	if (timer_thread) capinv_int_snd(timer_thread, rs);
}

void
chal_timer_thd_init(struct thread *t)
{ timer_thread = t; }

void
timer_init(u32_t frequency)
{
    u32_t divisor = PIT_SCALE / frequency;

    printk("Enabling timer\n");

    outb(PIT_CONTROL, PIT_SET);
    outb(PIT_A, divisor & PIT_MASK);
    outb(PIT_A, (divisor >> 8) & PIT_MASK);
}
