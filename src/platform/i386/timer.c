#include <thd.h>
#include <inv.h>
#include <hw.h>

#include "isr.h"
#include "io.h"
#include "kernel.h"

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

/*
 * These addressess are specified as offsets from the base HPET
 * pointer, which is a 1024-byte region of memory-mapped
 * registers. The reason we use offsets rather than a struct or
 * bitfields is that ALL accesses, both read and write, must be
 * aligned at 32- or 64-bit boundaries and must read or write an
 * entire 32- or 64-bit value at a time. Packed structs cause GCC to
 * produce code which attempts to operate on the single byte level,
 * which fails.
 */

#define HPET_OFFSET(n) ((unsigned char*)hpet + n)

#define HPET_CAPABILITIES  (0x0)
#define HPET_CONFIGURATION (0x10)
#define HPET_INTERRUPT     (0x20)
#define HPET_COUNTER       (*(u64_t*)(HPET_OFFSET(0xf0)))

#define HPET_T0_CONFIG    (0x100)
#define HPET_Tn_CONFIG(n) HPET_OFFSET(HPET_T0_CONFIG + (0x20 * n))

#define HPET_T0_COMPARATOR    (0x108)
#define HPET_Tn_COMPARATOR(n) HPET_OFFSET(HPET_T0_COMPARATOR + (0x20 * n))

#define HPET_T0_INTERRUPT    (0x110)
#define HPET_Tn_INTERRUPT(n) HPET_OFFSET(HPET_T0_INTERRUPT + (0x20 * n))

#define HPET_ENABLE_CNF (1ll)
#define HPET_LEG_RT_CNF (1ll<<1)

#define HPET_TAB_LENGTH  (0x4)
#define HPET_TAB_ADDRESS (0x2c)

/* Bits in HPET_Tn_CONFIG */
/* 1 << 0 is reserved */
#define TN_INT_TYPE_CNF	(1ll << 1)	/* 0 = edge trigger, 1 = level trigger */
#define TN_INT_ENB_CNF	(1ll << 2)	/* 0 = no interrupt, 1 = interrupt */
#define TN_TYPE_CNF	(1ll << 3)	/* 0 = one-shot, 1 = periodic */
#define TN_PER_INT_CAP	(1ll << 4)	/* read only, 1 = periodic supported */
#define TN_SIZE_CAP	(1ll << 5)	/* 0 = 32-bit, 1 = 64-bit */
#define TN_VAL_SET_CNF	(1ll << 6)	/* set to allow directly setting accumulator */
/* 1 << 7 is reserved */
#define TN_32MODE_CNF	(1ll << 8)	/* 1 = force 32-bit access to 64-bit timer */
/* #define TN_INT_ROUTE_CNF (1<<9:1<<13)*/	/* routing for interrupt */
#define TN_FSB_EN_CNF	(1ll << 14)	/* 1 = deliver interrupts via FSB instead of APIC */
#define TN_FSB_INT_DEL_CAP	(1ll << 15)	/* read only, 1 = FSB delivery available */

#define HPET_INT_ENABLE(n) (0x1 << n)	/* Clears the INT n for level-triggered mode. */

static volatile u64_t *hpet_config;
static volatile u64_t *hpet_interrupt;

volatile struct hpet_timer {
		u64_t config;
		u64_t compare;
		u64_t interrupt;
		u64_t reserved;
} __attribute__((packed)) *hpet_timers;

static void *hpet;
static u32_t tick = 0;

int
periodic_handler(struct pt_regs *regs)
{
	u64_t cycle;
	int preempt = 1;
	struct thread *timer_thd = hw_thd[HW_PERIODIC];

	rdtscll(cycle);
	tick++;
	printk("p"); /* comment this line for microbenchmarking tests */

	ack_irq(HW_PERIODIC);
	if (timer_thd) preempt = capinv_int_snd(timer_thd, regs);

	*hpet_interrupt = HPET_INT_ENABLE(TIMER_PERIODIC);

	return preempt;
}

int
oneshot_handler(struct pt_regs *regs)
{
	u64_t cycle;
	int preempt = 1;
	struct thread *timer_thd = hw_thd[HW_ONESHOT];

	rdtscll(cycle);
	printk("o"); /* comment this line for microbenchmarking tests */

	ack_irq(HW_ONESHOT);
	if (timer_thd) preempt = capinv_int_snd(timer_thd, regs);

	*hpet_interrupt = HPET_INT_ENABLE(TIMER_ONESHOT);

	return preempt;
}

void
timer_set(timer_type_t timer_type, u64_t cycles)
{
	u64_t outconfig = TN_INT_TYPE_CNF | TN_INT_ENB_CNF;
	int timer = 0;

	/* Disable timer interrupts */
	*hpet_config ^= ~1;

	/* Reset main counter */

	if (timer_type == TIMER_ONESHOT) {
		/* Set a static value to count up to */
		timer = 1;
		hpet_timers[timer].config = outconfig;
		cycles += HPET_COUNTER;
	} else {
		/* Set a periodic value */
		hpet_timers[timer].config = outconfig | TN_TYPE_CNF | TN_VAL_SET_CNF;
		/* Reset main counter */
		HPET_COUNTER = 0x00;
	}
	hpet_timers[timer].compare = cycles;

	/* Enable timer interrupts */
	*hpet_config |= 1;
}

u64_t
timer_find_hpet(void *timer)
{
	u32_t i;
	unsigned char sum = 0;
	unsigned char *hpetaddr = timer;
	u32_t length = *(u32_t*)(hpetaddr + HPET_TAB_LENGTH);

	printk("Initiliazing HPET @ %p\n", hpetaddr);

	for (i = 0; i < length; i++) {
		sum += hpetaddr[i];
	}

	if (sum == 0) {
		u64_t addr = *(u64_t*)(hpetaddr + HPET_TAB_ADDRESS);
		printk("-- Checksum is OK\n");
		printk("Addr: %016llx\n", addr);
		hpet = (void*)((u32_t)(addr & 0xffffffff));
		printk("hpet: %p\n", hpet);
		return addr;
	}

	printk("-- Invalid checksum (%d)\n", sum);
	return 0;
}

void
timer_set_hpet_page(u32_t page)
{
	hpet = (void*)(page * (1 << 22) | ((u32_t)hpet & ((1<<22)-1)));
	hpet_config = (u64_t*)((unsigned char*)hpet + HPET_CONFIGURATION);
	hpet_interrupt = (u64_t*)((unsigned char*)hpet + HPET_INTERRUPT);
	hpet_timers = (struct hpet_timer*)((unsigned char*)hpet + HPET_T0_CONFIG);
	printk("Set HPET @ %p\n", hpet);
}

void
timer_init(timer_type_t timer_type, u64_t cycles)
{
	printk("Enabling timer @ %p\n", hpet);

	/* Enable legacy interrupt routing */
	*hpet_config |= (1ll);

	/* Set the timer as specified */
	timer_set(timer_type, cycles);
}
