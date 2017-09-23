/* Simple PCI driver, only works for IVSHMEM device */
/* PCI background information can be obtained from OSDev http://wiki.osdev.org/PCI */
/* IVSHMEM device information can be found from qemu documentation */

#include <consts.h>
#include <assert.h>
#include "io.h"
#include "../../../kernel/include/chal.h"
#include "pci.h"
#include <llprint.h>


struct pci_device devices[PCI_DEVICE_NUM];
int dev_num = 0;

static inline u32_t
pci_read(u32_t bus, u32_t dev, u32_t func, u32_t reg)
{
	u32_t v = PCI_ADDR(bus, dev, func, reg);
	outl(PCI_CONFIG_ADDRESS, v);
	return inl(PCI_CONFIG_DATA);
}

static inline void
pci_write(u32_t bus, u32_t dev, u32_t func, u32_t reg, u32_t v)
{
	u32_t a = PCI_ADDR(bus, dev, func, reg);
	outl(PCI_CONFIG_ADDRESS, a);
	outl(PCI_CONFIG_DATA, v);
}

void
pci_print(void)
{
    int i;
	printc("total pci device %d\n", dev_num);
	for(i=0; i<dev_num; i++) {
		printc("%x:%x.%x vendor %x device %x class %x\n", devices[i].bus, devices[i].dev, devices[i].func, devices[i].vendor, devices[i].device, devices[i].classcode);
    }
}

/* int */
/* pci_vendor_populate(u16_t *vendors, size_t num_devices, struct pci_device **devices_p) */
/* { */
/*     int i; */
/*     struct pci_device *devices; */

/*     if (!devices_p || !vendors) return -1; */

/*     //bump allocator */
/*     devices = malloc(sizeof(pci_device) * size); */

/*     for (i = 0; i < size; i++) { */
/*     } */

/*     *devices_p = devices; */
/* } */

static void
pci_probe(void)
{
	int i, j, k, f;
	u32_t reg;
	for(i=0; i<PCI_BUS_MAX; i++) {
		for(j=0; j<PCI_DEVICE_MAX; j++) {
			for(f=0; f<PCI_FUNC_MAX; f++) {
				reg = pci_read(i, j, f, 0x0);
				if (reg == 0xFFFFFFFF) continue;
				for(k=0; k<PCI_DATA_NUM; k++) devices[dev_num].data[k] = pci_read(i, j, f, k<<2);
				devices[dev_num].bus       = (u32_t)i;
				devices[dev_num].dev       = (u32_t)j;
				devices[dev_num].func      = (u32_t)f;
				devices[dev_num].vendor    = (u16_t)PCI_VENDOR_ID(devices[dev_num].data[0]);
				devices[dev_num].device    = (u16_t)PCI_DEVICE_ID(devices[dev_num].data[0]);
				devices[dev_num].classcode = (u8_t)PCI_CLASS_ID(devices[dev_num].data[2]);
				devices[dev_num].subclass  = (u8_t)PCI_SUBCLASS_ID(devices[dev_num].data[2]);
				devices[dev_num].progIF    = (u8_t)PCI_PROG_IF(devices[dev_num].data[2]);
				devices[dev_num].header    = (u8_t)PCI_HEADER(devices[dev_num].data[3]);
				for(k=0; k<PCI_BAR_NUM; k++) devices[dev_num].bar[k].raw = devices[dev_num].data[4+k];
				dev_num++;
			}
		}
	}
    pci_print();

		/* Implementation detail is from PCI Local Bus Specification  */
		/* http://www.xilinx.com/Attachment/PCI_SPEV_V3_0.pdf */
		/* if (devices[i].vendor == PCI_IVSHMEM_VENDOR && devices[i].device == PCI_IVSHMEM_DEVICE) { */
		/* 	pci_write(devices[i].bus, devices[i].dev, devices[i].func, 6<<2, 0xFFFFFFFF); */
		/* 	reg = pci_read(devices[i].bus, devices[i].dev, devices[i].func, 6<<2); */
		/* 	pci_write(devices[i].bus, devices[i].dev, devices[i].func, 6<<2, devices[i].bar[2].raw); */
		/* 	ivshmem_phy_addr = devices[i].bar[2].raw & 0xFFFFFFF0; */
		/* 	ivshmem_sz = (~(reg & 0xFFFFFFF0))+1; */
		/* 	if (ivshmem_sz > IVSHMEM_TOT_SIZE) ivshmem_sz = IVSHMEM_TOT_SIZE; */
		/* } */
}

void
pci_init(void)
{
	pci_probe();
}
