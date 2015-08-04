#include "kernel.h"
#include "string.h"
#include "mem_layout.h"
#include "pgtbl.h"

typedef struct {
	char signature[8];
	u8_t checksum;
	char oemid[6];
	u8_t revision;
	u32_t rsdtaddress;
	u32_t length;
	u64_t xsdtaddress;
	u8_t extendedchecksum;
	u8_t reserved[3];
} __attribute__((packed)) RSDP;

typedef struct rsdt {
	char signature[4];
	u32_t length;
	u8_t revision;
	u8_t checksum;
	char oemid[6];
	char oemtableid[8];
	u32_t oemrevision;
	u32_t creatorid;
	u32_t creatorrevision;
	struct rsdt *entry[0];
} __attribute__((packed)) RSDT;

extern u8_t *boot_comp_pgd;
u32_t basepage;
static RSDT *rsdt;

static inline void *
pa2va(void *pa)
{
	return (void*)(((u32_t)pa & ((1<<22)-1)) | basepage);
}

void *
acpi_find_rsdt(void)
{
	char *sig;
	RSDP *rsdp;
	for (sig = (char*)0xc00E0000; sig < (char*)0xc00FFFFF; sig += 16) {
		if (!strncmp("RSD PTR ", sig, 8)) {
			break;
		}
	}
	rsdp = (RSDP*)sig;

	rsdt = (RSDT*)rsdp->rsdtaddress;
	return rsdt;
}

void *
acpi_find_timer(void)
{
        pgtbl_t pgtbl = (pgtbl_t)boot_comp_pgd;

	size_t i;
	for (i = 0; i < (rsdt->length - sizeof(RSDT)) / sizeof(RSDT*); i++) {
		RSDT *e = pa2va(rsdt->entry[i]);
		if (e->signature[0] == 'H' && e->signature[1] == 'P' &&
			e->signature[2] == 'E' && e->signature[3] == 'T')
		{
			return e;
		}
	}

	return NULL;
}


void
acpi_set_rsdt_page(u32_t page)
{
	basepage = page * (1 << 22);
	rsdt = (RSDT*)pa2va(rsdt);
}
