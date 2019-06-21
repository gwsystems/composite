#include "kernel.h"
#include "string.h"
#include "mem_layout.h"
#include "pgtbl.h"

#define RSDP_LO_ADDRESS ((unsigned char *)0xc00E0000)
#define RSDP_HI_ADDRESS ((unsigned char *)0xc00FFFFF)
#define RSDP_ALIGNMENT (16)

struct rsdp {
	char  signature[8];
	u8_t  checksum;
	char  oemid[6];
	u8_t  revision;
	u32_t rsdtaddress;
	u32_t length;
	u64_t xsdtaddress;
	u8_t  extendedchecksum;
	u8_t  reserved[3];
} __attribute__((packed));

struct acpi_header {
		char  sig[4];
		u32_t len;
};

struct rsdt {
	struct acpi_header head;
	u8_t         revision;
	u8_t         checksum;
	char         oemid[6];
	char         oemtableid[8];
	u32_t        oemrevision;
	u32_t        creatorid;
	u32_t        creatorrevision;
	paddr_t      entry[0];
} __attribute__((packed));

static struct rsdt *rsdt;

void *
acpi_find_rsdt(void)
{
	unsigned char *sig;
	struct rsdp   *rsdp = NULL;
	paddr_t        rsdt_pa;

	for (sig = RSDP_LO_ADDRESS; sig < RSDP_HI_ADDRESS; sig += RSDP_ALIGNMENT) {
		if (!strncmp("RSD PTR ", (char *)sig, 8)) {
			struct rsdp * r   = (struct rsdp *)sig;
			unsigned char sum = 0;
			u32_t         i;

			for (i = 0; i < r->length; i++) {
				sum += sig[i];
			}

			if (sum == 0) {
				printk("\tFound good RSDP @ %p\n", sig);
				rsdp = (struct rsdp *)sig;
				break;
			} else {
				printk("\tFound RSDP signature but bad checksum (%d) @ %p\n", sum, sig);
			}
		}
	}

	if (!rsdp) return NULL;

	rsdt_pa = (paddr_t)rsdp->rsdtaddress;
	return device_map_mem(rsdt_pa, 0);
}

static int
acpi_chk_header(void *ptr, const char *sig)
{
	struct acpi_header *h = ptr;
	char *mem;
	char  sum = 0;
	u32_t i;

	if (strncmp(h->sig, sig, 4)) return -1;
	mem = ptr;

	for (i = 0; i < h->len; i++) {
		sum += mem[i];
	}
	if (sum != 0) {
		printk("\tChecksum of acpi resource failed (with chksum %d)\n", sum % 255);
		return -1;
	}

	return 0;
}


static void *
acpi_find_resource_flags(const char *res_name, pgtbl_flags_t flags)
{
	size_t  i;

	for (i = 0; i < (rsdt->head.len - sizeof(struct rsdt)) / sizeof(struct rsdt *); i++) {
		struct rsdt *e = (struct rsdt *)device_pa2va(rsdt->entry[i]);

		if (!e) {
			/*
			 * I don't think this should commonly happen
			 * as the resources should be on the same
			 * super-page as the parent rsdt.
			 */
			e = device_map_mem(rsdt->entry[i], flags);
			assert(e);
		}
		if (!acpi_chk_header(e, res_name)) return e;
	}

	return NULL;
}

static void *
acpi_find_resource(const char *res_name)
{
	return acpi_find_resource_flags(res_name, 0);
}

void *
acpi_find_timer(void)
{
	return acpi_find_resource("HPET");
}

void *
acpi_find_apic(void)
{
	return acpi_find_resource_flags("APIC", PGTBL_NOCACHE);
}

/*
 * Thanks to kaworu @ https://forum.osdev.org/viewtopic.php?t=16990
 * for shutdown code:
 */
struct facp {
	struct acpi_header head;
	char   unneeded1[40 - 8];
	u32_t *dsdt;
	char   unneeded2[48 - 44];
	u32_t *smi_cmd;
	u8_t   acpi_enable;
	u8_t   acpi_disable;
	char   unneeded3[64 - 54];
	u32_t *pm1a_cnt_blk;
	u32_t *pm1b_cnt_blk;
	u8_t   unneeded4[89 - 72];
	u8_t   pm1_cnt_len;
} __attribute__((packed));

#define DSDT_HEADER_SZ 36

void
outw(unsigned short __val, unsigned short __port)
{
	__asm__ volatile("outw %0,%1" : : "a" (__val), "dN" (__port));
}

static void
outb(unsigned char __val, unsigned short __port)
{
	__asm__ volatile("outb %0,%1" : : "a" (__val), "dN" (__port));
}

static unsigned short
inw(unsigned short __port)
{
	unsigned short __val;

	__asm__ volatile("inw %1,%0" : "=a" (__val) : "dN" (__port));

	return __val;
}

void
acpi_shutdown(void)
{
	struct facp *facp = acpi_find_resource("FACP");
	char        *s5_addr;
	int          dsdt_len;
	struct acpi_header *dsdt;

	u32_t *smi_cmd;
	u8_t   acpi_enable;
	u8_t   acpi_disable;
	u32_t *pm1a_cnt;
	u32_t *pm1b_cnt;
	u16_t  slp_type_a;
	u16_t  slp_type_b;
	u16_t  slp_en;
	u8_t   pm1_cnt_len;

	if (!facp) return;

	dsdt = (struct acpi_header *)facp->dsdt;
	if (acpi_chk_header(dsdt, "DSDT")) return;

	dsdt_len = dsdt->len - DSDT_HEADER_SZ;
	s5_addr  = (char *)facp->dsdt + DSDT_HEADER_SZ;
	while (0 < dsdt_len--) {
		if (!strncmp(s5_addr, "_S5_", 4)) break;
		s5_addr++;
	}
	if (dsdt_len == 0) return;

	/* check if \_S5 was found and that there is a valid AML structure */
	if (!((*(s5_addr-1) == 0x08 || (*(s5_addr-2) == 0x08 && *(s5_addr-1) == '\\')) && *(s5_addr+4) == 0x12)) return;

	/* Great, lets find the values to feed into ACPI to reboot */
	s5_addr += 5;
	s5_addr += ((*s5_addr &0xC0) >> 6) + 2; /* calculate PkgLength size */

	if (*s5_addr == 0x0A) s5_addr++; /* skip byteprefix */
	slp_type_a = *(s5_addr) << 10;
	s5_addr++;

	if (*s5_addr == 0x0A) s5_addr++; /* skip byteprefix */
	slp_type_b = *(s5_addr) << 10;

	smi_cmd = facp->smi_cmd;

	acpi_enable  = facp->acpi_enable;
	acpi_disable = facp->acpi_disable;

	pm1a_cnt = facp->pm1a_cnt_blk;
	pm1b_cnt = facp->pm1b_cnt_blk;

	pm1_cnt_len = facp->pm1_cnt_len;

	slp_en = 1<<13;

	/* Enable ACPI. Check if acpi is already enabled */
	if (!inw((unsigned int) pm1a_cnt)) {
		u32_t i;
		u32_t spin_until = ~0;

		/* check if acpi can be enabled */
		if (smi_cmd == 0 || acpi_enable == 0) return;

		outb((unsigned int)smi_cmd, acpi_enable); /* send acpi enable command */
		/* takes up to 3 seconds time to enable acpi? */
		for (i = 0; i < spin_until; i++) {
			if (inw((unsigned int)pm1a_cnt) == 1) break;
			//sleep(10);
		}
		if (pm1b_cnt != 0) {
			for (; i < spin_until; i++ ) {
				if (inw((unsigned int)pm1b_cnt) == 1) break;
				//sleep(10);
			}
		}
		if (i == spin_until) return; /* couldn't enable in time */
	}


	/* Found everything we need from the dsdt, now shutdown! */
	outw((unsigned int)pm1a_cnt, slp_type_a | slp_en );
	if (pm1b_cnt != 0) {
		outw((unsigned int)pm1b_cnt, slp_type_b | slp_en);
	}

	return;
}

extern unsigned long kernel_mapped_offset;

void
acpi_init(void)
{
	u32_t page;
	void *timer;
	void *apic;
	int lapic_err = 1;
	void *hpet = NULL;
	unsigned long j = kernel_mapped_offset;

	boot_state_assert(INIT_UT_MEM);
	assert(j < PAGE_SIZE / sizeof(unsigned long));

	/* FIXME: Ugly hack to get the physical page with the ACPI RSDT mapped */
	printk("ACPI initialization\n");
	rsdt = NULL;
	rsdt = acpi_find_rsdt(); /* update global */
	if (!rsdt) {
		printk("\tCould not find the ACPI RSDT; not using ACPI.");
		return; 	/* No acpi? */
	}

	timer = acpi_find_timer();
	if (timer) {
		hpet = timer_initialize_hpet(timer);
	}
	if (!hpet) {
		printk("Could not initialize HPET.\n");
	}

	apic = acpi_find_apic();
	if (apic) {
		lapic_err = lapic_find_localaddr(apic);
	}
	assert(!lapic_err);
}
