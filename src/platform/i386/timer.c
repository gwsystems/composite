#include "isr.h"
#include "io.h"
#include "kernel.h"

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

typedef struct {
  u8_t sig[4];
  u32_t length;
  u8_t revision;
  u8_t checksum;
  u8_t oemid[6];
  u64_t oemtableid;
  u32_t oemrevision;
  u8_t creatorid[4];
  u32_t creatorrevision;
  struct {
    u8_t hwrev;
    u8_t ncomp :5;
    u8_t count_size_cap :1;
    u8_t reserved :1;
    u8_t legacy_irq :1;
    u16_t pci_vendor;
  } blockid;
  struct {
    u8_t space_id;
    u8_t reg_bit_width;
    u8_t reg_bit_offset;
    u8_t reserved;
    u64_t address;
  } address;
  u8_t number;
  u16_t minimumclocktick;
  u8_t oemattribute;
} __attribute__((packed)) HPET_tab;

volatile u64_t *hpet_cap;
volatile u64_t *hpet_config;
volatile u64_t *hpet_interrupt;
volatile u64_t *hpet_counter;

volatile struct hpet_timer {
		u64_t config;
		u64_t compare;
		u64_t interrupt;
		u64_t reserved;
} __attribute__((packed)) *hpet_timers;

static void *hpet;

static u32_t tick = 0;
static int current_type = TIMER_FREQUENCY;
static u64_t oneshot_target = 0;
static u64_t timerout = 0;

void
timer_hex(int start, int end)
{
	int i = 0;
	u32_t *b = (u32_t *)hpet;
	for (i = start / 8; i <= end / 8; i++) {
		printk("%08x%c", b[i], i - (start/8) % 4 == 3 ? '\n' : ' '); 
	}
}

void
timer_print(void)
{
	int i = 0;
	int ncounters = 1; /*(*hpet_cap >> 8) & 0x3f;*/

	printk("- HPET @ %p\n", hpet);
	printk("-- Capabilities (");
	timer_hex(0x0, 0x8);
	printk(")\n--- REV_ID:             %u\n", *hpet_cap & 0xff);
	printk("--- NUM_TIM_CAP:        %u\n", (*hpet_cap >> 8) & 0x3f);
	printk("--- COUNT_SIZE_CAP:     %u\n", (*hpet_cap >> 13) & 1);
	printk("--- LEG_ROUTE_CAP:      %u\n", (*hpet_cap >> 15) & 1);
	printk("--- VENDOR_ID:          %x\n", (*hpet_cap >> 16) & 0xffff);
	printk("--- COUNTER_CLK_PERIOD: %u\n", (*hpet_cap >> 32) & 0xffffffff);

	printk("-- Configuration (");
	timer_hex(0x10, 0x18);
	printk(")\n--- ENABLE_CNF: %d\n", *hpet_config & 1);
	printk("--- LEG_RT_CNF: %d\n", (*hpet_config >> 1) & 1);

	printk("-- Interrupt Status (");
	timer_hex(0x20, 0x28);
	printk(")\n");
	for (i = 0; i < ncounters; i++) {
		printk("--- T%d_INT_STS: %d\n", i, (*hpet_interrupt >> i) & 1);
	}

	printk("-- Counter (");
	timer_hex(0xf0, 0xf8);
	printk(")\n--- MAIN_COUNTER_VAL: %llu\n", *hpet_counter);

	for (i = 0; i < ncounters; i++) {
		u64_t cfg = hpet_timers[i].config;
		printk("-- Timer %d\n", i);
		printk("--- TN_INT_TYPE_CNF:    %u\n", (cfg >> 1) & 1);
		printk("--- TN_INT_ENB_CNF:     %u\n", (cfg >> 2) & 1);
		printk("--- TN_TYPE_CNF:        %u\n", (cfg >> 3) & 1);
		printk("--- TN_PER_INT_CAP:     %u\n", (cfg >> 4) & 1);
		printk("--- TN_SIZE_CAP:        %u\n", (cfg >> 5) & 1);
		printk("--- TN_VAL_SET_CNF:     %u\n", (cfg >> 6) & 1);
		printk("--- TN_32MODE_CNF:      %u\n", (cfg >> 8) & 1);
		printk("--- TN_INT_ROUTER_CNF:  %u\n", (cfg >> 9) & 0x20);
		printk("--- TN_FSB_EN_CNF:      %u\n", (cfg >> 14) & 1);
		printk("--- TN_FSB_INT_DEL_CAP: %u\n", (cfg >> 15) & 1);
		printk("--- TN_INT_ROUTE_CAP:   %x\n", (cfg >> 32));
	}

	timer_hex(0, 0x120);
}

void
timer_callback(struct registers *regs)
{
    u64_t cycle;
    rdtscll(cycle);
    tick++;

    /* timer_print(); */

    if (tick < 15) {
        printk("Tick: %2u @%10llu (%10llu)\n", tick, cycle, *hpet_counter);
    }

    if (current_type == TIMER_ONESHOT) {
	timerout *= 2;
      	timer_set(TIMER_ONESHOT, timerout);
    }

    *hpet_interrupt = 1;
    *hpet_config |= 1;
}

void
timer_set(int timer_type, u64_t cycles)
{
	u64_t outconfig = (1ll << 1) | (1ll << 2) | (1ll << 6);

	/* Disable timer interrupts */
	*hpet_config ^= ~1;

	/* Reset main counter */
	*hpet_counter = 0;

	/*
	printk("Setting timer 0:\n");
	printk("- Before:\n");
	timer_print();
	*/

	if (timer_type == TIMER_ONESHOT) {
		/* Set a static value to count up to */
		hpet_timers[0].config = outconfig;
	} else {
		/* Set a periodic value */
		hpet_timers[0].config = outconfig | (1ll << 3);
	}
	hpet_timers[0].compare = cycles;

	/* Save the current type of timer */
	current_type = timer_type;

	/*
	printk("- After:\n");
	timer_print();
	*/

	/* Enable timer interrupts */
	*hpet_config |= 1;
}

u64_t
timer_find_hpet(void *timer)
{
	u32_t i;
	unsigned char sum = 0;

	HPET_tab *hpetaddr = timer;
	printk("Initiliazing HPET @ %p\n", hpetaddr);
	/*
	printk("-- Signature:  %c%c%c%c\n", hpetaddr->sig[0], hpetaddr->sig[1], hpetaddr->sig[2], hpetaddr->sig[3]);
	printk("-- Length:     %d\n", hpetaddr->length);
	printk("-- Revision:   %d\n", hpetaddr->revision);
	printk("-- Checksum:   %x\n", hpetaddr->checksum);
	printk("-- OEM ID:     %c%c%c%c%c%c\n", hpetaddr->oemid[0], hpetaddr->oemid[1], hpetaddr->oemid[2], hpetaddr->oemid[3], hpetaddr->oemid[4], hpetaddr->oemid[5]);
	printk("-- OEM Rev:    %d\n", hpetaddr->oemrevision);
	printk("-- Creator ID: %c%c%c%c\n", hpetaddr->creatorid[0], hpetaddr->creatorid[1], hpetaddr->creatorid[2], hpetaddr->creatorid[3]);
	printk("-- CreatorRev: %d\n", hpetaddr->creatorrevision);
	printk("-- HW Revi:    %d\n", hpetaddr->blockid.hwrev);
	printk("-- N Compar:   %d\n", hpetaddr->blockid.ncomp);
	printk("-- Count Size: %d\n", hpetaddr->blockid.count_size_cap);
	printk("-- Reserved:   %d\n", hpetaddr->blockid.reserved);
	printk("-- Legacy IRQ: %d\n", hpetaddr->blockid.legacy_irq);
	printk("-- PCI Vendor: %hx\n", hpetaddr->blockid.pci_vendor);
	printk("-- AddrSpace:  %s (%d)\n", hpetaddr->address.space_id ? "I/O" : "Memory", hpetaddr->address.space_id);
	printk("-- Bit Width:  %d\n", hpetaddr->address.reg_bit_width);
	printk("-- Bit Offset: %d\n", hpetaddr->address.reg_bit_offset);
	printk("-- Reserved:   %d\n", hpetaddr->address.reserved);
	printk("-- Address:    %llx\n", hpetaddr->address.address);
	printk("-- Number:     %d\n", hpetaddr->number);
	printk("-- Min Tick:   %hu\n", hpetaddr->minimumclocktick);
	printk("-- OEM Attr:   %x\n", hpetaddr->oemattribute);
	*/
	for (i = 0; i < hpetaddr->length; i++) {
		sum += ((unsigned char*)hpetaddr)[i];
	}
	if (sum == 0) {
		printk("-- Checksum is OK\n");
		hpet = (void*)((u32_t)(hpetaddr->address.address & 0xffffffff));
		return hpetaddr->address.address;
	}

	printk("-- Invalid checksum (%d)\n", sum);
	return 0;
}

void
timer_set_hpet_page(u32_t page)
{
	hpet = (void*)(page * (1 << 22) | ((u32_t)hpet & ((1<<22)-1)));
	hpet_cap = (u64_t*)((char*)hpet + 0x0);
	hpet_config = (u64_t*)((char*)hpet + 0x10);
	hpet_interrupt = (u64_t*)((char*)hpet + 0x20);
	hpet_counter = (u64_t*)((char*)hpet + 0xf0);
	hpet_timers = (struct hpet_timer*)((char*)hpet + 0x100);
	printk("Set HPET @ %p\n", hpet);
	/* timer_print(); */
}


void
timer_init(int timer_type, u64_t cycles)
{
	printk("Enabling timer @ %p\n", hpet);
	register_interrupt_handler(IRQ0, timer_callback);
	register_interrupt_handler(IRQ2, timer_callback);

	/* Enable legacy interrupt routing */
	*hpet_config |= (1ll);

	/* TESTING: Debug 15 timer ticks */
	printk("T0: %llu\n", *hpet_counter);
	printk("T1: %llu\n", *hpet_counter);
	
	timerout = 1000;
	timer_set(TIMER_FREQUENCY, timerout);
	__asm__("sti");
	while (tick < 15) { __asm__("hlt"); }
	__asm__("cli");

	/* Set the timer as specified */
	timer_set(timer_type, cycles);
}
