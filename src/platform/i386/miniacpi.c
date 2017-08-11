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

struct rsdt {
	char         signature[4];
	u32_t        length;
	u8_t         revision;
	u8_t         checksum;
	char         oemid[6];
	char         oemtableid[8];
	u32_t        oemrevision;
	u32_t        creatorid;
	u32_t        creatorrevision;
	struct rsdt *entry[0];
} __attribute__((packed));

extern u8_t *       boot_comp_pgd;
static u32_t        basepage;
static struct rsdt *rsdt;

static inline void *
pa2va(void *pa)
{
	return (void *)(((u32_t)pa & ((1 << 22) - 1)) | basepage);
}

void *
acpi_find_rsdt(void)
{
	unsigned char *sig;
	struct rsdp *  rsdp = NULL;

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

	if (rsdp) {
		rsdt = (struct rsdt *)rsdp->rsdtaddress;
	} else {
		rsdt = NULL;
	}

	return rsdt;
}

void *
acpi_find_timer(void)
{
	pgtbl_t pgtbl = (pgtbl_t)boot_comp_pgd;
	size_t  i;

	for (i = 0; i < (rsdt->length - sizeof(struct rsdt)) / sizeof(struct rsdt *); i++) {
		struct rsdt *e = (struct rsdt *)pa2va(rsdt->entry[i]);
		if (!strncmp(e->signature, "HPET", 4)) {
			unsigned char *check = (unsigned char *)e;
			unsigned char  sum   = 0;
			u32_t          j;

			for (j = 0; j < e->length; j++) {
				sum += check[j];
			}

			if (sum != 0) {
				printk("\tChecksum of HPET @ %p failed (got %d)\n", e, sum % 255);
				continue;
			}

			printk("\tFound good HPET @ %p\n", e);
			return e;
		}
	}

	return NULL;
}

void *
acpi_find_apic(void)
{
	size_t i;

	for (i = 0; i < (rsdt->length - sizeof(struct rsdt)) / sizeof(struct rsdt *); i++) {
		struct rsdt *e = (struct rsdt *)pa2va(rsdt->entry[i]);
		if (!strncmp(e->signature, "APIC", 4)) {
			unsigned char *check = (unsigned char *)e;
			unsigned char  sum   = 0;
			u32_t          j;

			for (j = 0; j < e->length; j++) {
				sum += check[j];
			}

			if (sum != 0) {
				printk("\tChecksum of APIC @ %p failed (got %d)\n", e, sum % 255);
				continue;
			}

			printk("\tFound good APIC @ %p\n", e);
			return e;
		}
	}

	return NULL;
}

void
acpi_set_rsdt_page(u32_t page)
{
	basepage = page * (1 << 22);
	rsdt     = (struct rsdt *)pa2va(rsdt);
}
