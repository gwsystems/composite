#include<llprint.h>
#include<pci.h>
#include "cos_dpdk_adapter.h"

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
	int ret, dev_nb ;

	dev_nb	= pci_dev_count();
	ret	= pci_scan(cos_pci_devices, dev_nb);

	pci_dev_print(cos_pci_devices, dev_nb);

	return ret;
}