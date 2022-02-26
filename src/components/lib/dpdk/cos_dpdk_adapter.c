#include<llprint.h>
#include<pci.h>
#include<io.h>
#include<cos_kernel_api.h>

#include<rte_bus_pci.h>

#include "cos_dpdk_adapter.h"

DECLARE_COS_PMD(e1000);

extern struct rte_pci_bus rte_pci_bus;

struct pci_dev cos_pci_devices[PCI_DEVICE_MAX];
int pci_dev_nb = 0;

int
cos_printc(const char *fmt, va_list ap)
{
	char    s[128];
	size_t  ret, len = 128;

	ret = vsnprintf(s, len, fmt, ap);
	cos_llprint(s, ret);

	return ret;
}

int
cos_printf(const char *fmt,...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = cos_printc(fmt, ap);
	va_end(ap);
	return ret;
}

int
cos_bus_scan(void)
{
	int ret;

	pci_dev_nb	= pci_dev_count();
	ret		= pci_scan(cos_pci_devices, pci_dev_nb);

	pci_dev_print(cos_pci_devices, pci_dev_nb);

	return ret;
}

/* Write PCI config space. */
int
cos_pci_write_config(const struct rte_pci_device *device,
		const void *buf, size_t len, off_t offset)
{
	const uint32_t *buffer;
	unsigned int w;

	if (!buf) return -1;
	buffer = (const uint32_t *)buf;

	for (w = 0; w <= len - 4; w += 4) {
		pci_config_write(device->addr.bus, device->addr.devid,
				device->addr.function, (uint32_t)offset + w, *buffer);
	}
	return w;
}

int
cos_pci_read_config(const struct rte_pci_device *device,
		void *buf, size_t len, off_t offset)
{
	uint32_t *buffer;
	unsigned int r;

	if (!buf) return -1;
	buffer = (uint32_t *)buf;

	for (r = 0; r <= len - 4; r += 4) {
		*buffer = pci_config_read(device->addr.bus, device->addr.devid,
				device->addr.function, (uint32_t)offset + r);
	}
	return r;
}

int cos_pci_scan(void)
{
	int i, j;
	struct rte_pci_device *pci_device_list, *rte_dev;
	struct pci_dev *cos_dev;

	cos_bus_scan();

	pci_device_list = malloc(sizeof(*rte_dev) * pci_dev_nb);

	if (!pci_device_list) return -1;

	memset(pci_device_list, 0, sizeof(*rte_dev) * pci_dev_nb);

	cos_printf("cos pci sanning\n");

	for (i = 0; i < pci_dev_nb; i++) {
		rte_dev = &pci_device_list[i];
		cos_dev = &cos_pci_devices[i];
		/* rte_dev->device = NULL; */
		rte_dev->addr.bus = cos_dev->bus;
		rte_dev->addr.devid = cos_dev->dev;
		rte_dev->addr.function = cos_dev->func;
		rte_dev->id.class_id = cos_dev->classcode;
		rte_dev->id.vendor_id = cos_dev->vendor;
		rte_dev->id.device_id = cos_dev->device;
		rte_dev->id.subsystem_vendor_id = PCI_ANY_ID;
		rte_dev->id.subsystem_device_id = PCI_ANY_ID;
		for (j = 0; j < PCI_MAX_RESOURCE; j++) {
			rte_dev->mem_resource[j].phys_addr = cos_dev->bar[j].raw & 0xFFFFFFF0;
			if (!cos_dev->bar[j].raw) continue;

			uint32_t buf = 0;
			uint8_t offset;
			buf = 0xFFFFFFFF;
			offset = (j + 4) << 2;
			cos_pci_write_config(rte_dev, &buf, sizeof(buf), offset);
			cos_pci_read_config(rte_dev, &buf, sizeof(buf), offset);
			buf = ~(buf & ~0xF) + 1;
			rte_dev->mem_resource[j].len = buf;
			buf = cos_dev->bar[j].raw;
			cos_pci_write_config(rte_dev, &buf, sizeof(buf), offset);
			rte_dev->mem_resource[j].addr = NULL; /* Has yet to be mapped */
		}
		rte_dev->max_vfs = 0;
		rte_dev->kdrv = RTE_PCI_KDRV_UIO_GENERIC;
		pci_name_set(rte_dev);
		rte_pci_add_device(rte_dev);
	}	

	return 0;
}

void *
cos_map_phys_to_virt(void *paddr, unsigned int size)
{
	return (void *)cos_hw_map(cos_compinfo_get(cos_defcompinfo_curr_get()), BOOT_CAPTBL_SELF_INITHW_BASE, (paddr_t)paddr, size);
}

int
cos_dpdk_init(int argc, char **argv)
{
	rte_pci_bus.bus.scan = cos_pci_scan;
	return rte_eal_init(argc, argv);
}
