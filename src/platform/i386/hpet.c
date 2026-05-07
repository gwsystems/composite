#include <thd.h>
#include <inv.h>
#include <hw.h>

#include "isr.h"
#include "kernel.h"
#include "chal/cpuid.h"

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

#define HPET_OFFSET(n) ((unsigned char *)hpet + n)

#define HPET_CAPABILITIES (0x0)
#define HPET_CONFIGURATION (0x10)
#define HPET_INTERRUPT (0x20)
#define HPET_COUNTER (*(u64_t *)(HPET_OFFSET(0xf0)))

#define HPET_T0_CONFIG (0x100)
#define HPET_Tn_CONFIG(n) HPET_OFFSET(HPET_T0_CONFIG + (0x20 * n))

#define HPET_T0_COMPARATOR (0x108)
#define HPET_Tn_COMPARATOR(n) HPET_OFFSET(HPET_T0_COMPARATOR + (0x20 * n))

#define HPET_T0_INTERRUPT (0x110)
#define HPET_Tn_INTERRUPT(n) HPET_OFFSET(HPET_T0_INTERRUPT + (0x20 * n))

#define HPET_ENABLE_CNF (1ll)
#define HPET_LEG_RT_CNF (1ll << 1)

#define HPET_TAB_LENGTH (0x4)
#define HPET_TAB_ADDRESS (0x2c)

/* Bits in HPET_Tn_CONFIG */
/* 1 << 0 is reserved */
#define TN_INT_TYPE_CNF (1ll << 1) /* 0 = edge trigger, 1 = level trigger */
#define TN_INT_ENB_CNF (1ll << 2)  /* 0 = no interrupt, 1 = interrupt */
#define TN_TYPE_CNF (1ll << 3)     /* 0 = one-shot, 1 = periodic */
#define TN_PER_INT_CAP (1ll << 4)  /* read only, 1 = periodic supported */
#define TN_SIZE_CAP (1ll << 5)     /* 0 = 32-bit, 1 = 64-bit */
#define TN_VAL_SET_CNF (1ll << 6)  /* set to allow directly setting accumulator */
/* 1 << 7 is reserved */
#define TN_32MODE_CNF (1ll << 8)           /* 1 = force 32-bit access to 64-bit timer */
/* #define TN_INT_ROUTE_CNF (1<<9:1<<13)*/ /* routing for interrupt */
#define TN_FSB_EN_CNF (1ll << 14)          /* 1 = deliver interrupts via FSB instead of APIC */
#define TN_FSB_INT_DEL_CAP (1ll << 15)     /* read only, 1 = FSB delivery available */

#define HPET_INT_ENABLE(n) (*hpet_interrupt = (0x1 << n)) /* Clears the INT n for level-triggered mode. */

struct cap_asnd hw_asnd_caps[HW_IRQ_TOTAL];

static volatile u32_t *hpet_capabilities;
static volatile u64_t *hpet_config;
static volatile u64_t *hpet_interrupt;
static void *          hpet;

volatile struct hpet_timer {
	u64_t config;
	u64_t compare;
	u64_t interrupt;
	u64_t reserved;
} __attribute__((packed)) * hpet_timers;

/*
 * When determining how many CPU cycles are in a HPET tick, we must
 * execute a number of periodic ticks (TIMER_CALIBRATION_ITER) at a
 * controlled interval, and use the HPET tick granularity to compute
 * how many CPU cycles per HPET tick there are.  Unfortunately, this
 * can be quite low (e.g. HPET tick of 10ns, CPU tick of 2ns) leading
 * to rounding error that is a significant fraction of the conversion
 * factor.
 *
 * Practically, this will lead to the divisor in the conversion being
 * smaller than it should be, thus causing timers to go off _later_
 * than they should.  Thus we use a multiplicative factor
 * (TIMER_ERROR_BOUND_FACTOR) to lessen the rounding error.
 *
 * All of the hardware is documented in the HPET specification @
 * http://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/software-developers-hpet-spec-1-0a.pdf
 */

#define PICO_PER_MICRO 1000000UL
#define FEMPTO_PER_PICO 1000UL
#define TIMER_CALIBRATION_ITER 256
#define TIMER_ERROR_BOUND_FACTOR 256
static int           timer_calibration_init   = 1;
static unsigned long timer_cycles_per_hpetcyc = TIMER_ERROR_BOUND_FACTOR;
static unsigned long cycles_per_tick;
static unsigned long hpetcyc_per_tick;
#define ULONG_MAX 4294967295UL
extern u32_t chal_msr_mhz;
static unsigned long pico_per_hpetcyc;

static inline u64_t
timer_cpu2hpet_cycles(u64_t cycles)
{
	unsigned long cyc;

	/* demote precision to enable word-sized math */
	cyc = (unsigned long)cycles;
	if (unlikely((u64_t)cyc < cycles)) cyc= ULONG_MAX;
	/* convert from CPU cycles to HPET cycles */
	cyc = (cyc / timer_cycles_per_hpetcyc) * TIMER_ERROR_BOUND_FACTOR;
	/* promote the precision to interact with the hardware correctly */
	cycles = cyc;

	return cycles;
}

static void
timer_disable(timer_type_t timer_type)
{
	/* Disable timer interrupts */
	*hpet_config &= ~HPET_ENABLE_CNF;

	/* Disable timer interrupt of timer_type */
	hpet_timers[timer_type].config  = 0;
	hpet_timers[timer_type].compare = 0;

	printk("Timer %d disabled\n", timer_type);

	/* Enable timer interrupts */
	*hpet_config |= HPET_ENABLE_CNF;
}

static void
timer_calibration(void)
{
	static int   cnt   = 0;
	static u64_t cycle = 0, tot = 0, prev;
	static u32_t apic_curr = 0, apic_tot = 0, apic_prev;

	/* calibration only on BSP */
	assert(get_cpuid() == INIT_CORE);

	prev      = cycle;
	apic_prev = apic_curr;
	rdtscll(cycle);
	apic_curr = lapic_get_ccr();

	if (cnt) {
		tot += cycle - prev;
		apic_tot += (apic_prev - apic_curr);
	}
	if (cnt >= TIMER_CALIBRATION_ITER) {
		assert(hpetcyc_per_tick);
		timer_calibration_init = 0;
		cycles_per_tick        = (unsigned long)(tot / TIMER_CALIBRATION_ITER);
		assert(cycles_per_tick > hpetcyc_per_tick);

		if (!lapic_timer_calibrated()) {
			u32_t cycs_to_apic_ratio = 0, apic_cycs_per_tick = 0;

			apic_cycs_per_tick = apic_tot / TIMER_CALIBRATION_ITER;
			assert(apic_cycs_per_tick);

			cycs_to_apic_ratio = cycles_per_tick / apic_cycs_per_tick;
			lapic_timer_calibration(cycs_to_apic_ratio);
		}

		/* Possibly significant rounding error here.  Bound by the factor */
		timer_cycles_per_hpetcyc = (TIMER_ERROR_BOUND_FACTOR * cycles_per_tick) / hpetcyc_per_tick;

		timer_disable(TIMER_PERIODIC);
		timer_disable(TIMER_PERIODIC);
	}
	cnt++;
}

int
chal_cyc_usec(void)
{
	if (!lapic_timer_calibrated()) return 0;

	return cycles_per_tick / TIMER_DEFAULT_US_INTERARRIVAL;
}


void print_hpet_timer_info(timer_type_t timer_type)
{
    if (!hpet_timers || !hpet_config || !hpet_capabilities) {
        printk("HPET not initialized!\n");
        return;
    }

    u64_t cfg = hpet_timers[timer_type].config;
    u64_t cmp = hpet_timers[timer_type].compare;
    u64_t irq = (cfg >> 9) & 0x1F;

    printk("=== HPET Timer Info ===\n");
    printk("Core: %d\n", get_cpuid());

    printk("HPET Global Configuration: 0x%016llx\n", *hpet_config);
    printk("  - Enable CNF (bit 0): %llu\n", (*hpet_config & HPET_ENABLE_CNF) != 0);
    printk("  - Legacy Routing (bit 1): %llu\n", (*hpet_config & HPET_LEG_RT_CNF) != 0);

    printk("Timer config register: 0x%016llx\n", cfg);
    printk("  - Interrupt Type (TN_INT_TYPE_CNF, bit 1): %llu\n", (cfg & TN_INT_TYPE_CNF) != 0);
    printk("  - Interrupt Enable (TN_INT_ENB_CNF, bit 2): %llu\n", (cfg & TN_INT_ENB_CNF) != 0);
    printk("  - Timer Type (TN_TYPE_CNF, bit 3): %s\n", (cfg & TN_TYPE_CNF) ? "Periodic" : "One-shot");
    printk("  - Periodic Capable (TN_PER_INT_CAP, bit 4): %llu\n", (cfg & TN_PER_INT_CAP) != 0);
    printk("  - Value Set Allowed (TN_VAL_SET_CNF, bit 6): %llu\n", (cfg & TN_VAL_SET_CNF) != 0);
    printk("  - FSB delivery enabled (bit 14): %llu\n", (cfg & TN_FSB_EN_CNF) != 0);
    printk("  - FSB delivery capable (bit 15): %llu\n", (cfg & TN_FSB_INT_DEL_CAP) != 0);
    printk("  - Interrupt Routing CNF (bits 9-13): %llu\n", irq);

    printk("Timer compare register: 0x%016llx (%llu)\n", cmp, cmp);
    printk("HPET main counter: 0x%016llx (%llu)\n", HPET_COUNTER, HPET_COUNTER);

    printk("HPET Capabilities register: 0x%08x\n", *hpet_capabilities);
    u64_t pico_per_tick = hpet_capabilities[1] / FEMPTO_PER_PICO;
    printk("HPET Tick granularity: %llu picoseconds\n", pico_per_tick);

    printk("=== End of HPET Timer Info ===\n");
}

//volatile int counter = 0;
int
periodic_handler(struct pt_regs *regs)
{
	int preempt = 1;

	if (unlikely(timer_calibration_init)) timer_calibration();

	ack_irq(HW_PERIODIC);
	lapic_ack();
	struct cap_asnd *asndc = &hw_asnd_caps[HW_PERIODIC];

	printk("HPET IRQ cpu=%d asndc=%p type=%d cpuid=%d arcv_cap=%d\n",
	 	get_cpuid(),
	 	asndc,
	 	asndc->h.type,
	 	asndc->cpuid,
		asndc->arcv_capid);

	preempt = cap_hw_asnd(asndc, regs);

	HPET_INT_ENABLE(TIMER_PERIODIC);

	return preempt;
}

extern int timer_process(struct pt_regs *regs);

volatile int print_lock = 0;

int
oneshot_handler(struct pt_regs *regs)
{
	int preempt = 1;

	// simple lock for print debug
    while (__atomic_test_and_set(&print_lock, __ATOMIC_ACQUIRE)); // busy wait

    printk("hpet Oneshot in core %d\n", get_cpuid()); //Made for old pic? may need to mask this IOapic to prevent doubles, but would require irq

    __atomic_clear(&print_lock, __ATOMIC_RELEASE);

	ack_irq(HW_ONESHOT);
	//printk("Ack'ack the lapic\n");
	lapic_ack();
	preempt = timer_process(regs);
	HPET_INT_ENABLE(TIMER_ONESHOT);

	//hpet_oneshot_test();

	return preempt;
}

void
timer_set(timer_type_t timer_type, u64_t cycles)
{
	u64_t outconfig = TN_INT_TYPE_CNF | TN_INT_ENB_CNF; 

	/* Disable timer interrupts */
	*hpet_config &= ~HPET_ENABLE_CNF;

	/* Reset main counter */
	if (timer_type == TIMER_ONESHOT) {
		cycles = timer_cpu2hpet_cycles(cycles);

		/* Set a static value to count up to */
		hpet_timers[timer_type].config = outconfig;
		cycles += HPET_COUNTER;
	} else {
		/* Set a periodic value */
		hpet_timers[timer_type].config = outconfig | TN_TYPE_CNF | TN_VAL_SET_CNF;
		/* Reset main counter */
		HPET_COUNTER = 0x00;
	}
	hpet_timers[timer_type].compare = cycles;

	/* Enable timer interrupts */
	*hpet_config |= HPET_ENABLE_CNF;

	printk("Timer set of type %d\n", timer_type);
}

void *
timer_initialize_hpet(void *timer)
{
	u32_t          i;
	unsigned char  sum      = 0;
	unsigned char *hpetaddr = timer;
	u32_t          length;
	u64_t          addr;

	assert(timer);
	printk("Initializing HPET @ %p\n", hpetaddr);

	length = *(u32_t *)(hpetaddr + HPET_TAB_LENGTH);
	for (i = 0; i < length; i++) {
		sum += hpetaddr[i];
	}

	if (sum != 0) {
		printk("\tInvalid checksum (%d)\n", sum);

		return 0;
	}

	addr = *(u64_t *)(hpetaddr + HPET_TAB_ADDRESS);
	printk("\tChecksum is OK\n");
	printk("\tAddr: %016llx\n", addr);
	hpet = device_map_mem((paddr_t)((u32_t)(addr & 0xffffffff)), 0);
	printk("\thpet: %p\n", hpet);

	hpet_capabilities = (u32_t *)((unsigned char *)hpet + HPET_CAPABILITIES);
	hpet_config       = (u64_t *)((unsigned char *)hpet + HPET_CONFIGURATION);
	hpet_interrupt    = (u64_t *)((unsigned char *)hpet + HPET_INTERRUPT);
	hpet_timers       = (struct hpet_timer *)((unsigned char *)hpet + HPET_T0_CONFIG);

	printk("\tSet HPET @ %p\n", hpet);

	return hpet;
}

u64_t
timer_us2hpet_cycles(unsigned int us)
{
	assert(pico_per_hpetcyc > 0);
	return ((u64_t)us * PICO_PER_MICRO) / pico_per_hpetcyc;
}

void
timer_set_periodic_us(unsigned int period_us)
{
	if(period_us == 0){
		timer_disable(TIMER_PERIODIC);
	} 
	else{
		timer_set(TIMER_PERIODIC, timer_us2hpet_cycles(period_us));
	}
	
}

void
timer_set_oneshot_us(unsigned int period_us)
{
	if(period_us == 0){
		timer_disable(TIMER_ONESHOT);
	} 
	else{
		timer_set(TIMER_ONESHOT, timer_us2hpet_cycles(period_us));
	}
}

void
timer_init(void)
{
	assert(hpet_capabilities);
	pico_per_hpetcyc = hpet_capabilities[1]
	                   / FEMPTO_PER_PICO; /* bits 32-63 are # of femptoseconds per HPET clock tick */
	assert(pico_per_hpetcyc > 0);
	hpetcyc_per_tick = (TIMER_DEFAULT_US_INTERARRIVAL * PICO_PER_MICRO) / pico_per_hpetcyc;

	printk("Enabling timer @ %p with tick granularity %ld picoseconds\n", hpet, pico_per_hpetcyc);
	/* Enable legacy interrupt routing */
	*hpet_config |= HPET_LEG_RT_CNF;

	/*
	 * Set the timer as specified.  This assumes that the cycle
	 * specification is in hpet cycles (not cpu cycles).
	 */
	if (chal_msr_mhz && !lapic_timer_calibrated()) {
		cycles_per_tick          = chal_msr_mhz * TIMER_DEFAULT_US_INTERARRIVAL;
		timer_cycles_per_hpetcyc = cycles_per_tick / hpetcyc_per_tick;
		printk("\tTimer calibrated using using MSR frequency value\n");
		timer_calibration_init = 0;

		return;
	}

	timer_set(TIMER_PERIODIC, hpetcyc_per_tick);
}

void
hpet_speed_test(void)
{
	printk("Starting HPET Speed test...\n");

	/* Enable timer interrupts */
    *hpet_config |= HPET_ENABLE_CNF;

    u64_t tsc_start, tsc_end; //tsc - time stamp counter
    u64_t hpet_start, hpet_end;

	u64_t pico_per_hpetcyc = hpet_capabilities[1] / FEMPTO_PER_PICO; //Convert femtoseconds to picoseconds

	u64_t hpet_freq = 1000000000000 / pico_per_hpetcyc; //ticks per second = picoseconds per second / picoseconds per tick

    rdtscll(tsc_start);

    hpet_start = HPET_COUNTER;

    /* wait 1 second using HPET */
    u64_t target = hpet_start + hpet_freq;

    while (HPET_COUNTER < target) {
        printk("HPET_COUNTER: %llu\n", HPET_COUNTER); //Removing this print breaks the test, ask why???
    }

    rdtscll(tsc_end);
    hpet_end = HPET_COUNTER;

    u64_t tsc_delta  = tsc_end - tsc_start;
    u64_t hpet_delta = hpet_end - hpet_start;

	u64_t cpu_hz = (tsc_delta * hpet_freq) / hpet_delta;
    
    printk("HPET frequency: %llu Hz\n", hpet_freq);
    printk("CPU frequency:  %llu MHz\n", cpu_hz / 1000000);

	/* Disable timer interrupts */
	*hpet_config &= ~HPET_ENABLE_CNF;

    printk("HPET Speed test complete.\n");
}

void
hpet_oneshot_test(void)
{

    /* Enable HPET */
    //*hpet_config |= HPET_ENABLE_CNF;

    u64_t pico_per_hpetcyc = hpet_capabilities[1] / FEMPTO_PER_PICO;
    u64_t hpet_freq = 1000000000000ULL / pico_per_hpetcyc;

    printk("Programming oneshot for 10 seconds (%llu HPET cycles)\n", hpet_freq);

    timer_set(TIMER_ONESHOT, hpet_freq * 10);

	print_hpet_timer_info(TIMER_ONESHOT);

	return;
}