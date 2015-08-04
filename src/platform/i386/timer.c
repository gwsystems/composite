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
  u32_t eventtimerblockid;
  u32_t base_address[3];
  u8_t number;
  u16_t minimumclocktick;
  u8_t oemattribute;
} __attribute__((packed)) HPET;


static u32_t tick = 0;
static int current_type = TIMER_FREQUENCY;
static u64_t oneshot_target = 0;

void
timer_callback(struct registers *regs)
{
    tick++;
    u64_t cycle;
    rdtscll(cycle);

    if (tick < 15) {
        printk("Tick: %2u @%10llu\n", tick, cycle);
    }

    if (current_type == TIMER_ONESHOT) {
         u64_t timer;
         rdtscll(timer);
         timer_set(TIMER_FREQUENCY, DEFAULT_FREQUENCY);
    }
    printk("\n");

    if (tick % 3 == 0) {
        timer_set(TIMER_ONESHOT, 2000000000);
    } else {
	printk("Next: %2u |%10llu\n", tick+1, cycle+DEFAULT_FREQUENCY);
    }
}

void
timer_set(int timer_type, u64_t cycles)
{
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
}

void
timer_init(void *timer, int timer_type, u64_t cycles)
{
    if (timer == NULL) {
        /* No HPET is available, fall back to 8254 */
    } else {
	HPET *hpet = timer;
	printk("Initiliazing HPET @ %p\n", hpet);
	printk("-- Signature:  %c%c%c%c\n", hpet->sig[0], hpet->sig[1], hpet->sig[2], hpet->sig[3]);
	printk("-- Length:     %d\n", hpet->length);
	printk("-- Revision:   %d\n", hpet->revision);
	printk("-- Checksum:   %x\n", hpet->checksum);
	printk("-- OEM ID:     %c%c%c%c%c%c\n", hpet->oemid[0], hpet->oemid[1], hpet->oemid[2], hpet->oemid[3], hpet->oemid[4], hpet->oemid[5]);
	printk("-- OEM Rev:    %d\n", hpet->oemrevision);
	printk("-- Creator ID: %c%c%c%c\n", hpet->creatorid[0], hpet->creatorid[1], hpet->creatorid[2], hpet->creatorid[3]);
	printk("-- CreatorRev: %d\n", hpet->creatorrevision);
	printk("-- Block ID:   %d\n", hpet->eventtimerblockid);
	printk("-- BaseAddr:   %p-%p-%p\n", hpet->base_address[0], hpet->base_address[1], hpet->base_address[2]);
	printk("-- Number:     %d\n", hpet->number);
	printk("-- Min Tick:   %hd\n", hpet->minimumclocktick);
	printk("-- OEM Attr:   %x\n", hpet->oemattribute);
    }
    printk("Enabling timer\n");
    register_interrupt_handler(IRQ0, timer_callback);

    timer_set(timer_type, cycles);

    __asm__("sti");
    while (tick < 15) { __asm__("hlt"); }
    __asm__("cli");
}
