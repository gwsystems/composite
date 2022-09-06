#include <consts.h>
#include <assert.h>
#include "io.h"
#include <pci.h>
#include <llprint.h>

u32_t
pci_config_read(u32_t bus, u32_t dev, u32_t func, u32_t reg)
{
	u32_t v = PCI_ADDR(bus, dev, func, reg);
	outl(PCI_CONFIG_ADDRESS, v);

	return inl(PCI_CONFIG_DATA);
}

void
pci_config_write(u32_t bus, u32_t dev, u32_t func, u32_t reg, u32_t v)
{
	u32_t a = PCI_ADDR(bus, dev, func, reg);
	outl(PCI_CONFIG_ADDRESS, a);
	outl(PCI_CONFIG_DATA, v);
}

int
pci_scan(struct pci_dev *devices, int sz)
{
	int i, j, k, f, tmp, dev_num;
	u32_t reg;
	struct pci_bar *bar;

	dev_num = 0;

	if (sz <= 0) return -1;

	for (i = 0 ; i < PCI_BUS_MAX ; i++) {
		for (j = 0 ; j < PCI_DEVICE_MAX ; j++) {
			for (f = 0 ; f < PCI_FUNC_MAX ; f++) {
				reg = pci_config_read(i, j, f, 0x0);
				if (reg == PCI_BITMASK_32) continue;
				for (k = 0 ; k < PCI_DATA_NUM ; k++) {
					devices[dev_num].data[k] = pci_config_read(i, j, f, k << 2);
				}
				
				devices[dev_num].bus       = (u32_t)i;
				devices[dev_num].dev       = (u32_t)j;
				devices[dev_num].func      = (u32_t)f;
				devices[dev_num].vendor    = (u16_t)PCI_VENDOR_ID(devices[dev_num].data[0]);
				devices[dev_num].device    = (u16_t)PCI_DEVICE_ID(devices[dev_num].data[0]);
				devices[dev_num].classcode = (u8_t)PCI_CLASS_ID(devices[dev_num].data[2]);
				devices[dev_num].subclass  = (u8_t)PCI_SUBCLASS_ID(devices[dev_num].data[2]);
				devices[dev_num].progIF    = (u8_t)PCI_PROG_IF(devices[dev_num].data[2]);
				devices[dev_num].header    = (u8_t)PCI_HEADER(devices[dev_num].data[3]);

				for (k = 0 ; k < PCI_BAR_NUM ; k++) {
					devices[dev_num].bar[k].raw = devices[dev_num].data[4+k];
					/* the least signficant 8 bits hold data about the region type, locatable and prefetchable, so they are masked out */
					devices[dev_num].bar[k].paddr = devices[dev_num].bar[j].raw & 0xFFFFFFF0;

				}
				
				dev_num++;
				if (dev_num >= sz || dev_num >= PCI_DEVICE_MAX) {
					return 0;
				}
			}
		}
	}

	return 0;
}

void
pci_dev_print(struct pci_dev *devices, int sz)
{
    int i;
	for (i = 0 ; i < sz ; i++) {
	 	printc("%x:%x.%x vendor %x device %x class %x\n", devices[i].bus, devices[i].dev, devices[i].func, devices[i].vendor, devices[i].device, devices[i].classcode);
    }
}

int
pci_dev_get(struct pci_dev *devices, int sz, struct pci_dev *dev, u16_t dev_id, u16_t vendor_id)
{
	int i;

	for (i = 0 ; i < sz ; i++) {
		if (devices[i].vendor == vendor_id && devices[i].device == dev_id) {
			*dev = devices[i];
			return 0;
		}
	}

	return -1;
}

int
pci_dev_count(void)
{
	int i, j, k, f, tmp, dev_num;
	u32_t reg;
	struct pci_bar *bar;

	dev_num = 0;

	for (i = 0 ; i < PCI_BUS_MAX ; i++) {
		for (j = 0 ; j < PCI_DEVICE_MAX ; j++) {
			for (f = 0 ; f < PCI_FUNC_MAX ; f++) {
				reg = pci_config_read(i, j, f, 0x0);
				if (reg == PCI_BITMASK_32) continue;
					dev_num++;
			}
		}
	}

	assert(dev_num <= PCI_DEVICE_MAX);

	return dev_num;
}
