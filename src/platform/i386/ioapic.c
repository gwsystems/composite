#include "kernel.h"
#include "ioapic.h"
#include "pic.h"

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

struct ioapic_info {
	unsigned int   ioapicid;
	volatile void *io_vaddr;
	int            nentries;
	int            glbint_base;
};

static struct ioapic_info ioapicinfo[IOAPIC_MAX] = { { 0, NULL, 0, 0} };
static unsigned int ioapic_count;

void
ioapic_set_page(struct ioapic_info *io, u32_t page)
{
        io->io_vaddr = (volatile u32_t *)(page * (1 << 22) | ((u32_t)io->io_vaddr & ((1 << 22) - 1)));

        printk("\tSet IOAPIC %d @ %p\n", io->ioapicid, io->io_vaddr);
}

static void
ioapic_reg_write(struct ioapic_info *io, u8_t offset, u32_t val)
{
        *(volatile u32_t *)(io->io_vaddr + IOAPIC_IOREGSEL) = offset;
        *(volatile u32_t *)(io->io_vaddr + IOAPIC_IOWIN)    = val;
}

static u32_t
ioapic_reg_read(struct ioapic_info *io, u8_t offset)
{
        *(volatile u32_t *)(io->io_vaddr + IOAPIC_IOREGSEL) = offset;

        return *(volatile u32_t *)(io->io_vaddr + IOAPIC_IOWIN);
}

void
ioapic_int_mask(int intnum)
{
	/* TODO */
}

void
ioapic_int_unmask(int intnum)
{
	/* TODO */
}

static struct ioapic_info *
ioapic_findbygsi(int irq)
{
	unsigned int i = 0;

	for (; i < ioapic_count; i++) {
		if (irq >= ioapicinfo[i].glbint_base && irq < ioapicinfo[i].nentries) return &ioapicinfo[i];
	}

	return NULL;
}

static struct ioapic_info *
ioapic_findbyid(int id)
{
	unsigned int i = 0;

	for (; i < ioapic_count; i++) {
		if (id == (int)(ioapicinfo[i].ioapicid)) return &ioapicinfo[i];
	}

	return NULL;
}

void
ioapic_int_override(struct intsrcovrride_cntl *iso)
{
	assert(iso->header.len == sizeof(struct intsrcovrride_cntl));

	if (iso->source != iso->glb_int_num_off) {
		struct ioapic_info *ioap = ioapic_findbygsi(iso->glb_int_num_off);

		assert(ioap);
		printk("\tInterrupt Source Override for [%u] => %u with IOAPIC %d\n", iso->source, iso->glb_int_num_off, ioap->ioapicid);
		ioapic_reg_write(ioap, IOAPIC_IOREDTBL_OFFSET(iso->glb_int_num_off), iso->source + HW_IRQ_START);
	}
}

void
ioapic_int_enable(int irqnum, int cpunum, int addflag)
{
	struct ioapic_info *ioap = ioapic_findbygsi(irqnum);

	assert(ioap);
	if (addflag) {
		/* TODO: logical destination = 1 and add core no or lapic number? */
	} else {
		ioapic_reg_write(ioap, IOAPIC_IOREDTBL_OFFSET(irqnum), irqnum + HW_IRQ_START);
		ioapic_reg_write(ioap, IOAPIC_IOREDTBL_OFFSET(irqnum)+1, cpunum<<24);
	}
}

void
ioapic_int_disable(int irqnum)
{
	struct ioapic_info *ioap = ioapic_findbygsi(irqnum);

	assert(ioap);
	ioapic_reg_write(ioap, IOAPIC_IOREDTBL_OFFSET(irqnum), IOAPIC_INT_DISABLED | irqnum);
	ioapic_reg_write(ioap, IOAPIC_IOREDTBL_OFFSET(irqnum)+1, 0);
}

void
ioapic_iter(struct ioapic_cntl *io)
{
	u32_t ver;
	int ioent, j;
	static int more = 0;

	assert(io);

	if (ioapic_count == IOAPIC_MAX) {
		more ++;
		printk("\t%d more than %d IOAPICs present..\n", more, IOAPIC_MAX);

		return;
	}
	
	ioapicinfo[ioapic_count].io_vaddr = (volatile void *)(io->ioapic_phys_addr);	
	ioapicinfo[ioapic_count].ioapicid = io->ioapic_id;
	ioapic_set_page(&(ioapicinfo[ioapic_count]), vm_set_supage((u32_t)(ioapicinfo[ioapic_count].io_vaddr)));

	ver   = ioapic_reg_read(&ioapicinfo[ioapic_count], IOAPIC_IOAPICVER);
	ioent = ((ver >> 16) & 0xFF) + 1;
	printk("\tIOAPIC %d (counter:%d): Number of entries = %d\n", io->ioapic_id, ioapic_count, ioent);

	ioapicinfo[ioapic_count].nentries    = ioent;
	ioapicinfo[ioapic_count].glbint_base = io->glb_int_num_off;
	ioapic_count ++;

	for (j = 0; j < ioent; j++) ioapic_int_enable(io->glb_int_num_off + j, 0, 0); /* TODO: assign to different cores */
}

void
ioapic_init(void)
{
	assert(ioapic_count);
	pic_disable();

	printk("Setting up IOAPIC (disabling PIC)\n");

	/*
	 * PCI Interrupts may need some attention here.
	 * TODO: Test it with NIC in RK env.
	 */
}
