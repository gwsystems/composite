#include "kernel.h"
#include "ioapic.h"
#include "pic.h"

#define IOAPIC_MAX 4
#define IOAPIC_INT_ISA_MAX 16 /* ACPI 5.0 spec: only ISA interrupts can have overrides */

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

union ioapic_int_redir_entry {
	struct {
		u64_t vector: 8;
		u64_t delivmod: 3;
		u64_t destmod: 1;
		u64_t delivsts: 1;
		u64_t polarity: 1;
		u64_t remoteirr: 1;
		u64_t trigger: 1;
		u64_t mask: 1;
		u64_t reserved: 39;
		u64_t destination: 8;
	};
	struct {
		u32_t low_dword;
		u32_t high_dword;
	};
};

struct ioapic_isa_override {
	int source;
	int gsi;
	union {
		struct {
			u16_t polarity:2;
			u16_t trigger:2;
			u16_t reserved:12;
		};
		u16_t flags;
	};
};

static struct ioapic_info ioapicinfo[IOAPIC_MAX] = { { 0, NULL, 0, 0} };
static unsigned int ioapic_count;
static struct ioapic_isa_override ioapic_isainfo[IOAPIC_INT_ISA_MAX];
static unsigned int ioapic_isaoverride_count;
static unsigned int ioapic_int_count;

static union ioapic_int_redir_entry ioapic_int_isa_tmpl = {
	.delivmod = IOAPIC_DELIV_FIXED,
	.destmod  = IOAPIC_DST_PHYSICAL,
	.polarity = IOAPIC_POL_ACTHIGH,
	.trigger  = IOAPIC_TRIGGER_EDGE,
	.mask     = 1,
};

static union ioapic_int_redir_entry ioapic_int_pci_tmpl = {
	.delivmod = IOAPIC_DELIV_FIXED,
	.destmod  = IOAPIC_DST_PHYSICAL,
	.polarity = IOAPIC_POL_ACTLOW,
	.trigger  = IOAPIC_TRIGGER_EDGE, /* ref. barrelfish doesn't use level */
	.mask     = 1,
};

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

static struct ioapic_info *
ioapic_findbygsi(int gsi)
{
	unsigned int i = 0;

	for (; i < ioapic_count; i++) {
		if (gsi >= ioapicinfo[i].glbint_base && gsi < ioapicinfo[i].nentries) return &ioapicinfo[i];
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

static inline void
ioapic_int_entry_write(struct ioapic_info *io, u8_t off, union ioapic_int_redir_entry entry)
{
	int tmpoff = IOAPIC_IOREDTBL_OFFSET(off);

	ioapic_reg_write(io, tmpoff, entry.low_dword);
	ioapic_reg_write(io, tmpoff+1, entry.high_dword);
}

static inline union ioapic_int_redir_entry
ioapic_int_entry_read(struct ioapic_info *io, u8_t off)
{
	union ioapic_int_redir_entry entry;
	int tmpoff = IOAPIC_IOREDTBL_OFFSET(off);

	entry.low_dword  = ioapic_reg_read(io, tmpoff);
	entry.high_dword = ioapic_reg_read(io, tmpoff+1);

	return entry;
}

static inline void
ioapic_int_mask_set(int gsi, int mask, int dest)
{
	struct ioapic_info *io = ioapic_findbygsi(gsi);
	union ioapic_int_redir_entry entry;
	u8_t off;

	if (!io) return;

	off = gsi - io->glbint_base;
	entry = ioapic_int_entry_read(io, off);
	entry.mask = mask ? 1 : 0;
	entry.destination = apicids[dest];
	ioapic_int_entry_write(io, off, entry);
	entry = ioapic_int_entry_read(io, off);
}

static inline int
ioapic_int_gsi(int gsi)
{
	int override_gsi = gsi;
	int i;

	if (gsi < IOAPIC_INT_ISA_MAX) {
		for (i = 0; i < (int)ioapic_isaoverride_count; i++) {
			if (ioapic_isainfo[i].source == gsi && ioapic_isainfo[i].gsi != gsi) {
				override_gsi = ioapic_isainfo[i].gsi;
				break;
			}
		}
	}

	return override_gsi;
}

void
ioapic_int_mask(int gsi)
{
	ioapic_int_mask_set(ioapic_int_gsi(gsi), 1, 0);
}

void
ioapic_int_unmask(int gsi, int dest)
{
	ioapic_int_mask_set(ioapic_int_gsi(gsi), 0, dest);
}

void
ioapic_int_override(struct intsrcovrride_cntl *iso)
{
	union ioapic_int_redir_entry entry = ioapic_int_isa_tmpl;
	struct ioapic_info *iogsi = NULL, *iosrc = NULL;

	assert(iso->header.len == sizeof(struct intsrcovrride_cntl));

	assert(iso->source < IOAPIC_INT_ISA_MAX);
	assert(ioapic_isaoverride_count < IOAPIC_INT_ISA_MAX);

	if (iso->source != iso->glb_int_num_off) {
		union ioapic_int_redir_entry srcentry = ioapic_int_isa_tmpl;

		iosrc = ioapic_findbygsi(iso->source);
		assert(iosrc);
		srcentry.vector = iso->glb_int_num_off + HW_IRQ_START;
		ioapic_int_entry_write(iosrc, iso->source - iosrc->glbint_base, srcentry);

		ioapic_isainfo[ioapic_isaoverride_count].source = iso->glb_int_num_off;
		ioapic_isainfo[ioapic_isaoverride_count].gsi    = iso->source;
		ioapic_isainfo[ioapic_isaoverride_count].flags  = 0;
		ioapic_isaoverride_count++;
	}

	ioapic_isainfo[ioapic_isaoverride_count].source = iso->source;
	ioapic_isainfo[ioapic_isaoverride_count].gsi    = iso->glb_int_num_off;
	ioapic_isainfo[ioapic_isaoverride_count].flags  = iso->flags;

	printk("\tINT Override %u to %u, polarity: %u trigger: %u\n", iso->source, iso->glb_int_num_off,
	       ioapic_isainfo[ioapic_isaoverride_count].polarity, ioapic_isainfo[ioapic_isaoverride_count].trigger);

	switch(ioapic_isainfo[ioapic_isaoverride_count].trigger) {
	case ACPI_MADT_ISO_TRIG_CONFORMS: break;
	case ACPI_MADT_ISO_TRIG_EDGE: entry.trigger = IOAPIC_TRIGGER_EDGE; break;
	case ACPI_MADT_ISO_TRIG_RESERVED: assert(0); break;
	case ACPI_MADT_ISO_TRIG_LEVEL: entry.trigger = IOAPIC_TRIGGER_EDGE; break; /* XXX: should be level */
	default: break;
	}

	switch(ioapic_isainfo[ioapic_isaoverride_count].polarity) {
	case ACPI_MADT_ISO_POL_CONFORMS: break;
	case ACPI_MADT_ISO_POL_ACTHIGH: entry.polarity = IOAPIC_POL_ACTHIGH; break;
	case ACPI_MADT_ISO_POL_RESERVED: assert(0); break;
	case ACPI_MADT_ISO_POL_ACTLOW: entry.polarity = IOAPIC_POL_ACTLOW; break;
	default: break;
	}

	entry.vector = iso->source + HW_IRQ_START;
	iogsi = ioapic_findbygsi(iso->glb_int_num_off);
	assert(iogsi);

	ioapic_int_entry_write(iogsi, iso->glb_int_num_off - iogsi->glbint_base, entry);

	ioapic_isaoverride_count++;
}

void
ioapic_iter(struct ioapic_cntl *io)
{
	u32_t ver;
	int ioent, j;
	static int more = 0;
	unsigned int tmp_count = ioapic_count;

	assert(io);

	if (ioapic_count == IOAPIC_MAX) {
		more ++;
		printk("\t%d more than %d IOAPICs present..\n", more, IOAPIC_MAX);

		return;
	}

	ioapic_count ++;
	ioapicinfo[tmp_count].io_vaddr = (volatile void *)(io->ioapic_phys_addr);
	ioapicinfo[tmp_count].ioapicid = io->ioapic_id;
	ioapic_set_page(&(ioapicinfo[tmp_count]), vm_map_superpage((u32_t)(ioapicinfo[tmp_count].io_vaddr), 0));

	ver   = ioapic_reg_read(&ioapicinfo[tmp_count], IOAPIC_IOAPICVER);
	ioent = ((ver >> 16) & 0xFF) + 1;
	printk("\tIOAPIC %d (counter:%d): Number of entries = %d\n", io->ioapic_id, tmp_count, ioent);

	ioapicinfo[tmp_count].nentries    = ioent;
	ioapicinfo[tmp_count].glbint_base = io->glb_int_num_off;
	ioapic_int_count += ioent;

	for (j = 0; j < ioent; j++) {
		union ioapic_int_redir_entry entry = (io->glb_int_num_off + j) < IOAPIC_INT_ISA_MAX ? ioapic_int_isa_tmpl : ioapic_int_pci_tmpl;

		entry.vector = io->glb_int_num_off + j + HW_IRQ_START;

		ioapic_int_entry_write(&ioapicinfo[tmp_count], j, entry);
	}
}

void
chal_irq_enable(int irq, int dest)
{
	if (irq - HW_IRQ_START >= (int)ioapic_int_count) return;
	ioapic_int_unmask(irq - HW_IRQ_START, dest);
}

void
chal_irq_disable(int irq)
{
	if (irq - HW_IRQ_START >= (int)ioapic_int_count) return;
	ioapic_int_mask(irq - HW_IRQ_START);
}

void
ioapic_init(void)
{
	assert(ioapic_count);
	pic_disable();

	printk("Setting up IOAPIC (disabling PIC)\n");

	/*
	 * PCI Interrupts may need some attention here.
	 * https://forum.osdev.org/viewtopic.php?f=1&t=21745
	 * The discussion in the above forum suggest modern PCIe devices bypass IOAPIC and send
	 * interrupts directly to the core. For legacy PCI, we probably need to read some APIC tables.
	 *
	 * Update: with BMK_SCREW_INTERRUPT_ROUTING, got Rumpkernel to boot fine on HW as well.
	 * The effect of that BMK_SCREW_INTERRUPT_ROUTING is mostly in the BMK intr.c to use an array of lists vs
	 * single list. It doesn't change how NetBSD does interrupt processing.
	 */
}
