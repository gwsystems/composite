#include "kernel.h"
#include "string.h"

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

typedef struct {
	char signature[4];
	u32_t length;
	u8_t revision;
	u8_t checksum;
	char oemid[6];
	char oemtableid[8];
	u32_t oemrevision;
	u32_t creatorid;
	u32_t creatorrevision;
} __attribute__((packed)) RSDT;

void *
acpi_find_rsdp(void)
{
	printk("\n\n-- Looking for the RSDP --\n\n");

	char *sig;
	RSDP *rsdp;
	for (sig = (char*)0x000E0000; sig < (char*)0x000FFFFF; sig += 16) {
		if (!strncmp("RSD PTR ", sig, 8)) {
			printk("At %p: '%s'\n", sig, sig);
			break;
		}
	}
	rsdp = (RSDP*)sig;

	printk("\n\n-- Looks like RSDP is at %p --\n\n", rsdp);
	printk("-- Signature: %c%c%c%c%c%c%c%c\n", rsdp->signature[0], rsdp->signature[1], rsdp->signature[2], rsdp->signature[3], rsdp->signature[4], rsdp->signature[5], rsdp->signature[6], rsdp->signature[7]);
	printk("-- Checksum: %x\n", rsdp->checksum);
	printk("-- OEM ID: %c%c%c%c%c%c\n", rsdp->oemid[0], rsdp->oemid[1], rsdp->oemid[2], rsdp->oemid[3], rsdp->oemid[4], rsdp->oemid[5]);
	printk("-- Revision: %u\n", rsdp->revision);
	printk("-- RSDT Addr: %x\n", rsdp->rsdtaddress);
	printk("-- Length: %u\n", rsdp->length);
	printk("-- XSDT Addr: %x\n", rsdp->xsdtaddress);
	printk("-- Ext Checksum: %x\n", rsdp->extendedchecksum);
	

	printk("\n\n-- Done with RSDP -- \n\n");

	RSDT *rsdt = (RSDT*)rsdp->rsdtaddress;

	printk("--- RSDT @ %p\n", rsdt);
	printk("--- signature: %c%c%c%c\n", rsdt->signature[0],rsdt->signature[1],rsdt->signature[2],rsdt->signature[3]);
	printk("--- length: %u\n", rsdt->length);
	printk("--- revision: %u\n", rsdt->revision);
	printk("--- checksum: %x\n", rsdt->checksum);
	printk("--- oemid: %c%c%c%c%c%c\n", rsdt->oemid[0],rsdt->oemid[1],rsdt->oemid[2],rsdt->oemid[3],rsdt->oemid[4],rsdt->oemid[5]);
	printk("--- oemtableid: %c%c%c%c%c%c%c%c\n", rsdt->oemtableid[0], rsdt->oemtableid[1], rsdt->oemtableid[2], rsdt->oemtableid[3], rsdt->oemtableid[4], rsdt->oemtableid[5], rsdt->oemtableid[6], rsdt->oemtableid[7]);
	printk("--- oemrevision: %u\n", rsdt->oemrevision);
	printk("--- creatorid: %x\n", rsdt->creatorid);
	printk("--- creatorrevision: %d\n", rsdt->creatorrevision);

	return rsdp;
}
