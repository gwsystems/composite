#include <cos_component.h>
#include <llprint.h>
#include <pci.h>
#include "hw_profile.h"

int error_print(u32_t index, u32_t received, u32_t expected, char* variable);
int error_check(int index, struct pci_dev received, struct pci_dev expected);

int
main(void)
{
	int size = pci_dev_count();
	int i;
	struct pci_dev my_devices[size];
	struct pci_dev my_dev;
	u32_t my_dev_id, my_dev_vendor;

	/* tests pci_dev_count() */
	if (size != 6) {
		printc("Failure: pci_num returned %d instead of 6 (QEMU version 1:2.11+dfsg-1ubuntu7.36)\n", size);
		return 0;
	}
	
	/* tests pci_scan() */
	if (pci_scan(my_devices, size) != 0) {
		printc("Failure: pci_scan returned non-0 value\n");
		return 0;
	}
	for (i = 0 ; i < size ; i++) {
		if(error_check(i, my_devices[i], all_dev[i]) == -1) {
			return -1;
		}
	}

	/* tests pci_dev_get */
	my_dev_id = my_devices[0].device;
	my_dev_vendor = my_devices[0].vendor;

	if (pci_dev_get(my_devices, size, &my_dev, my_dev_id, my_dev_vendor) != 0) {
		printc("pci_dev_get failed (QEMU version 1:2.11+dfsg-1ubuntu7.36)\n");
		return -1;
	}
	if (my_dev.device != my_dev_id || my_dev.vendor != my_dev_vendor) {
		printc("pci_dev_get failed, device or vendor id does not match (QEMU version 1:2.11+dfsg-1ubuntu7.36)\n");
		return -1;
	}

	/* tests pci_dev_print */
	pci_dev_print(my_devices, size);

	printc("Success (QEMU version 1:2.11+dfsg-1ubuntu7.36)\n");
	while(1);

	return 0;

}

int 
error_check(int index, struct pci_dev received, struct pci_dev expected)
{
	if (received.bus != expected.bus) {
		return error_print(index, received.bus, expected.bus, "bus");
	}
	if (received.dev != expected.dev) {
		return error_print(index, received.dev, expected.dev, "dev");
	}
	if (received.func != expected.func) {
		return error_print(index, received.func, expected.func, "func");		
	}
	if (received.vendor != expected.vendor) {
		return error_print(index, received.vendor, expected.vendor, "vendor");
	}
	if (received.device != expected.device) {
		return error_print(index, received.device, expected.device, "device");		
	}
	if (received.classcode != expected.classcode) {
		return error_print(index, received.classcode, expected.classcode, "classcode");		
	}
	if (received.subclass != expected.subclass) {
		return error_print(index, received.subclass, expected.subclass, "subclass");		
	}
	if (received.progIF != expected.progIF) {
		return error_print(index, received.progIF, expected.progIF, "progIF");		
	}
	if (received.header != expected.header) {
		return error_print(index, received.header, expected.header, "device");		
	}

	return 0;
}

int
error_print(u32_t index, u32_t received, u32_t expected, char* variable)
{
	printc("Failure: got %x instead of %x for devices[%d] %s (QEMU version 1:2.11+dfsg-1ubuntu7.36)\n", received, expected, index, variable);
	return -1;
}