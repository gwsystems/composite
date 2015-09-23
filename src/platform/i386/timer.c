#define ENABLE_TIMER

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

void
timer_handler(struct pt_regs *rs)
{
	static long tick = 0;

	ack_irq(32);
	if (tick % 100 == 0) {
		printk("Tick: %lu\n", tick);
	}
	++tick;
}

void
timer_init(u32_t frequency)
{
    u32_t divisor = PIT_SCALE / frequency;

    printk("Enabling timer\n");

    outb(PIT_CONTROL, PIT_SET);
    outb(PIT_A, divisor & PIT_MASK);
    outb(PIT_A, (divisor >> 8) & PIT_MASK);
}
