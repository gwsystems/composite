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


static void
pci_scan_type_dev(struct pci_dev *dev)
{
	int i;
	u32_t pci_reg_curr, pci_reg_next;
	u64_t len;

	/* scan bars */
	for (i = 0; i < PCI_BAR_NUM; i++) {
		/* raw value of this bar */
		pci_reg_curr = (i + PCI_BAR_START) << 2;
		dev->bar[i].raw = pci_config_read(dev->bus, dev->dev, dev->func, pci_reg_curr);
		dev->bar[i].bar_type = dev->bar[i].raw & 0x1;
		dev->bar[i].len = 0;

		if (dev->bar[i].raw == 0) {
			/* This bar cannot be used */
			dev->bar[i].accessibility = 0;
			continue;
		}

		switch (dev->bar[i].bar_type) {
		case PCI_BAR_MEM:
			if (dev->bar[i].mem.memType == PCI_MEM_BAR_64) {
				assert(i + 1 < PCI_BAR_NUM);
				pci_reg_next = (i + 1 + PCI_BAR_START) << 2;
				dev->bar[i + 1].raw = pci_config_read(dev->bus, dev->dev, dev->func, pci_reg_next); 

				/* 
				 * paddr is combined of 2 bars if it is a 64-bit type. The
				 * least signficant 4 bits hold data about the region type,
				 * locatable and prefetchable, so they are masked out
				 */
				dev->bar[i].paddr = (dev->bar[i].raw & 0xFFFFFFF0) | (u64_t)dev->bar[i + 1].raw << 32;

				/*
				 * Determine the lenth of this bar memory:
				 * 1. write all 1's to the two bars
				 * 2. read the curr bar value and clear the least signficant 4 bits as above
				 * 3. read the next bar value and move it into the high 32-bit of len
				 * 4. reverse all bits in len
				 * 5. add 1 to len and we know the lenth of this bar
				 */
				pci_config_write(dev->bus, dev->dev, dev->func, pci_reg_curr, ~0x0);
				pci_config_write(dev->bus, dev->dev, dev->func, pci_reg_next, ~0x0);
				len = pci_config_read(dev->bus, dev->dev, dev->func, pci_reg_curr) & ~0xF;
				len = len | (u64_t)(pci_config_read(dev->bus, dev->dev, dev->func, pci_reg_next)) << 32; 
				len = ~len;
				len += 1;

				dev->bar[i].len = len;

				/* must write back of the raw values because of the previous read lenth algorithm */
				pci_config_write(dev->bus, dev->dev, dev->func, pci_reg_curr, dev->bar[i].raw);
				pci_config_write(dev->bus, dev->dev, dev->func, pci_reg_next, dev->bar[i + 1].raw); 

				/* mark the accessibility of the two bars, user should not use the second bar */
				dev->bar[i].accessibility = 1;
				dev->bar[i + 1].accessibility = 0;

				/* skip the next bar */
				i += 1;
			} else {
				/* process algorithm is similar to 64-bit mem type */
				dev->bar[i].paddr = dev->bar[i].raw & 0xFFFFFFF0;
				dev->bar[i].accessibility = 1;

				pci_config_write(dev->bus, dev->dev, dev->func, pci_reg_curr, ~0x0);
				len = pci_config_read(dev->bus, dev->dev, dev->func, pci_reg_curr) & ~0xF;
				len = ~(u32_t)len;
				len += 1;

				dev->bar[i].len = len;
				pci_config_write(dev->bus, dev->dev, dev->func, pci_reg_curr, dev->bar[i].raw);
			}
			break;
		case PCI_BAR_IO:
			/* TODO: process the IO bar */
			dev->bar[i].accessibility = 0;
			break;
		default:
			break;
		}
	}

	dev->subsystem_device_id = (u16_t)PCI_SUBSYSTEM_ID(dev->data[0xB]);
	dev->subsystem_vendor_id = (u16_t)PCI_SUBSYSTEM_VENDOR_ID(dev->data[0xB]);
}

/* helper function to deal with different pci types */
static void
pci_scan_helper(struct pci_dev *dev)
{
	assert(dev);
	int i;

	switch (dev->pci_type) {
	case PCI_TYPE_DEVICE:
		pci_scan_type_dev(dev);
		break;
	case PCI_TYPE_PCI_TO_PCI_BRIDGE:
		/* TODO: process this pci type */
		break;
	case PCI_TYPE_PCI_TO_CARDBUS_BRIDGE:
		/* TODO: process this pci type */
		break;
	default:
		/* invalid pci device */
		assert(0);
		break;
	}
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

				/* First common contents of this pci device */
				for (k = 0 ; k < PCI_COMMON_DATA_SZ; k++) {
					devices[dev_num].data[k] = pci_config_read(i, j, f, k << 2);
				}
				
				/* These contents are held by all types of pci device */
				devices[dev_num].bus       = (u32_t)i;
				devices[dev_num].dev       = (u32_t)j;
				devices[dev_num].func      = (u32_t)f;
				devices[dev_num].vendor    = (u16_t)PCI_VENDOR_ID(devices[dev_num].data[0]);
				devices[dev_num].device    = (u16_t)PCI_DEVICE_ID(devices[dev_num].data[0]);
				devices[dev_num].classcode = (u8_t)PCI_CLASS_ID(devices[dev_num].data[2]);
				devices[dev_num].subclass  = (u8_t)PCI_SUBCLASS_ID(devices[dev_num].data[2]);
				devices[dev_num].progIF    = (u8_t)PCI_PROG_IF(devices[dev_num].data[2]);
				devices[dev_num].header    = (u8_t)PCI_HEADER(devices[dev_num].data[3]);

				/* Then read other data by pci header type */
				pci_scan_helper(&devices[dev_num]);
				
				dev_num++;
				assert(dev_num <= sz);
			}
		}
	}

	return 0;
}

void
pci_dev_print(struct pci_dev *devices, int sz)
{
	int i, j;
	for (i = 0 ; i < sz ; i++) {
	 	printc("%x:%x.%x vendor %x device %x class %x ", devices[i].bus, devices[i].dev, devices[i].func, devices[i].vendor, devices[i].device, devices[i].classcode);
		if (devices[i].pci_type == PCI_TYPE_DEVICE) {
			printc("type %s\n", "PCI_TYPE_DEVICE");
			for (j = 0; j < PCI_BAR_NUM; j++) {
				if(!devices[i].bar[j].accessibility || devices[i].bar[j].bar_type != PCI_BAR_MEM) continue;
				printc("\tbar[%d]: type %s, paddr %llx, len %llu\n", j, devices[i].bar[j].mem.memType == PCI_MEM_BAR_32 ? "32-bit" : "64-bit", devices[i].bar[j].paddr, devices[i].bar[j].len);
			}
		} else if (devices[i].pci_type == PCI_TYPE_PCI_TO_PCI_BRIDGE) {
			printc("type: %s\n", "PCI_TYPE_PCI_TO_PCI_BRIDGE");
		} else if (devices[i].pci_type == PCI_TYPE_PCI_TO_CARDBUS_BRIDGE) {
			printc("type: %s\n", "PCI_TYPE_PCI_TO_CARDBUS_BRIDGE");
		} else {
			printc("Invalid pci device\n");
		}
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
	assert(dev_num <= PCI_DEVICE_NUM);

	return dev_num;
}
