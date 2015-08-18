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

/* Select Channel: */
#define CHANNEL0    0x00
#define CHANNEL1    0x40
#define CHANNEL2    0x80
#define READBACK    0xc0
/* Access Mode: */
#define LATCHCOUNT  0x00
#define LOBYTE      0x10
#define HIBYTE      0x20
#define LOHIBYTE    0x30
/* Operating Mode: */
#define MODE0       0x00	// Interrupt on terminal count
#define MODE1       0x02	// Hardware re-triggerable one-shot
#define MODE2       0x04	// Rate generator
#define MODE3       0x06	// Square wave generator
#define MODE4       0x08	// Software triggered strobe
#define MODE5       0x0a	// Hardware triggered strobe
//alias MODE2       0x0c
//alias MODE3       0x0e
/* BCD/Binary: */
#define BINARY      0x00
#define BCD         0x01

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

typedef struct {
  u8_t sig[4];
  u32_t length;
  u8_t revision;
  u8_t checksum;
  u8_t oemid[6];
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

typedef struct {
	u64_t config;
	u64_t interrupt;
	u64_t counter;
	struct {
		u64_t config;
		u64_t compare;
		u64_t interrupt;
	} timers[0];
} __attribute__((packed)) HPET;

static HPET *hpet;

static u32_t tick = 0;
static int current_type = TIMER_FREQUENCY;
static u64_t oneshot_target = 0;
static u64_t timerout = 0;

void
timer_callback(struct registers *regs)
{
    tick++;
    u64_t cycle;
    rdtscll(cycle);

    if (tick < 25) {
        printk("Tick: %2u @%10llu\n", tick, cycle);
	timerout *= 10;
    }

    if (current_type == TIMER_ONESHOT) {
         u64_t timer;
         rdtscll(timer);
         timer_set(TIMER_FREQUENCY, DEFAULT_FREQUENCY);
    }

    timer_set(TIMER_ONESHOT, timerout);
}

void
timer_set(int timer_type, u64_t cycles)
{
#if 0
    u64_t timer;
    u8_t mode;

    switch (timer_type) {
        case TIMER_FREQUENCY:
            timer = /* PIT_SCALE / */ cycles;
            mode = MODE3;
            break;

        case TIMER_ONESHOT:
            rdtscll(timer);
            timer += cycles;
            oneshot_target = timer;
            printk("Oneshot: +%10llu\n", cycles);
            printk("Goal: %2u !%10llu\n", tick+1, oneshot_target);
            mode = MODE4;
            break;

        default:
            printk("Invalid timer type: %d\n");
            assert(0);
            break;
    }

    current_type = timer_type;

    outb(PIT_CONTROL, mode | CHANNEL0 | LOHIBYTE | BINARY);
    outb(PIT_C, timer & PIT_MASK);
    outb(PIT_C, (timer >> 8) & PIT_MASK);
#else
    if (timer_type == TIMER_ONESHOT) {
        hpet->timers[0].config = 1 << 2;
    } else {
        hpet->timers[0].config = (1 << 2) | (1 << 3) | (1 << 6);
    }
    hpet->timers[0].interrupt = cycles + hpet->counter;
#endif
}

u64_t
timer_find_hpet(void *timer)
{
	HPET_tab *hpetaddr = timer;
	printk("Initiliazing HPET @ %p\n", hpetaddr);
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
	hpet = (HPET*)((u32_t)(hpetaddr->address.address & 0xffffffff));
	return hpetaddr->address.address;
}

void
timer_set_hpet_page(u32_t page)
{
	hpet = (HPET*)(page * (1 << 22) | ((u32_t)hpet & ((1<<22)-1)));
	printk("Set HPET @ %p\n", hpet);
	printk("-- Config:           %llx\n", hpet->config);
	printk("-- Interrupt Status: %llx\n", hpet->interrupt);
	printk("-- Counter:          %llx\n", hpet->counter);
}

void
timer_init(int timer_type, u64_t cycles)
{
    printk("Enabling timer @ %p\n", hpet);
    register_interrupt_handler(IRQ0, timer_callback);

    timer_set(timer_type, cycles);
    timerout = cycles;

    __asm__("sti");
    while (tick < 15) { __asm__("hlt"); }
    __asm__("cli");
}
