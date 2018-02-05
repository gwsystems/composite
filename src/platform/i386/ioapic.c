#include "kernel.h"
#include "ioapic.h"

#define IOAPIC_MAX 4

#define IOAPIC_IOAPICID  0x00
#define IOAPIC_IOAPICVER 0x01
#define IOAPIC_IOAPICARB 0x02

#define IOAPIC_IOREGSEL 0x00
#define IOAPIC_IOWIN    (IOAPIC_IOREGSEL + 0x10)
#define IOAPIC_IOREDTBL 0x10
#define IOAPIC_IOREDTBL_OFFSET(n) (IOAPIC_IOREDTBL + 2*n)

#define IOAPIC_INT_DISABLED (1<<16)

enum ioapic_deliverymode
{
        IOAPIC_DELIV_FIXED  = 0,
        IOAPIC_DELIV_LOWEST = 1,
        IOAPIC_DELIV_SMI    = 2,
        IOAPIC_DELIV_NMI    = 4,
        IOAPIC_DELIV_INIT   = 5,
        IOAPIC_DELIV_EXTINT = 7,
};

enum ioapic_dstmode
{
        IOAPIC_DST_PHYSICAL = 0,
        IOAPIC_DST_LOGICAL  = 1,
};

enum ioapic_pinpolarity
{
        IOAPIC_POL_ACTHIGH = 0,
        IOAPIC_POL_ACTLOW  = 1,
};

enum ioapic_triggermode
{
        IOAPIC_TRIGGER_EDGE  = 0,
        IOAPIC_TRIGGER_LEVEL = 1,
};


static volatile void *ioapic_base = (volatile void *)0xfec00000;
static unsigned int ioapic_count;

void
ioapic_set_page(u32_t page)
{
        ioapic_base = (volatile u32_t *)(page * (1 << 22) | ((u32_t)ioapic_base & ((1 << 22) - 1)));

        printk("\tSet IOAPIC @ %p\n", ioapic_base);
}

static void
ioapic_reg_write(u8_t offset, u32_t val)
{
        *(volatile u32_t *)(ioapic_base + IOAPIC_IOREGSEL) = offset;
        *(volatile u32_t *)(ioapic_base + IOAPIC_IOWIN)    = val;
}

static u32_t
ioapic_reg_read(u8_t offset)
{
        *(volatile u32_t *)(ioapic_base + IOAPIC_IOREGSEL) = offset;

        return *(volatile u32_t *)(ioapic_base + IOAPIC_IOWIN);
}

void
ioapic_int_mask(int intnum)
{
        /*
         * TODO:
         * 1. how to find which IOAPIC ? 
         * 2. read register (only first 32bits) for that redirection entry.
         * 3. mask that bit and write that register back.
         */
}

void
ioapic_int_unmask(int intnum)
{
        /*
         * TODO:
         * 1. how to find which IOAPIC ? 
         * 2. read register (only first 32bits) for that redirection entry.
         * 3. unmask that bit and write that register back.
         */
}

void
ioapic_int_override(struct intsrcovrride_cntl *iso)
{
	int src, gsi;

	assert(iso->header.len == sizeof(struct intsrcovrride_cntl));
	printk("\tInterrupt Source Override for [%u] => %u\n", iso->source, iso->glb_int_num_off);

	/* TODO: Find the right IOAPIC based on the GSI base in IOAPIC info */

	if (iso->source != iso->glb_int_num_off) {
		ioapic_reg_write(IOAPIC_IOREDTBL_OFFSET(iso->glb_int_num_off), iso->source + 32);
	}
}

void
ioapic_int_enable(int irqnum, int cpunum, int addflag)
{
	if (addflag) {
		/* TODO: logical destination = 1 and add core no or lapic number? */
	} else {
		ioapic_reg_write(IOAPIC_IOREDTBL_OFFSET(irqnum), irqnum + 32);
		ioapic_reg_write(IOAPIC_IOREDTBL_OFFSET(irqnum)+1, cpunum<<24);
	}
}

void
ioapic_int_disable(int irqnum)
{
        ioapic_reg_write(IOAPIC_IOREDTBL_OFFSET(irqnum), IOAPIC_INT_DISABLED | irqnum);
        ioapic_reg_write(IOAPIC_IOREDTBL_OFFSET(irqnum)+1, 0);
}

void
ioapic_iter(struct ioapic_cntl *io)
{
	u32_t ver, ioent, i;

	assert(io);

	ioapic_count ++;
	
	/* FIXME: Just one for now! */
	if (ioapic_count > 1) return;

	ioapic_base = (volatile u32_t *)(io->ioapic_phys_addr);
	ioapic_set_page(vm_set_supage((u32_t)ioapic_base));

	ver   = ioapic_reg_read(IOAPIC_IOAPICVER);
	ioent = ((ver >> 16) & 0xFF) + 1;

	printk("\tIOAPIC %d: Number of entries = %d\n", io->ioapic_id, ioent);

	for (i = 0; i < ioent; i++) ioapic_int_enable(i, 0, 0);
}
