/*** 
 * This defines the expected devices on the PCI bus, which is checked against in the test file pci.c
 * WARNING: THIS ASSUMES THE USER IS RUNNING QEMU version 1:2.11+dfsg-1ubuntu7.36
 * If you are NOT using QEMU version 1:2.11+dfsg-1ubuntu7.36, then the devices may be different,
 * causing the test to fail, even if the code is working correctly
*/

#include <pci.h>

struct pci_dev all_dev[] = {
		{
			.bus = 0,	
			.dev = 0,	
			.func = 0,	
			.vendor = 0x8086,	
			.device = 0x1237,	
			.classcode = 6,	
			.subclass = 0,	
			.progIF = 0,	
			.header = 0,	
			.data = {},	
			.index = 0,	
			.drvdata = 0
		},
		{
			.bus = 0,	
			.dev = 1,	
			.func = 0,	
			.vendor = 0x8086,	
			.device = 0x7000,	
			.classcode = 6,	
			.subclass = 1,	
			.progIF = 0,	
			.header = 0x80,	
			.data = {},	
			.index = 0,	
			.drvdata = 0
		},
		{
			.bus = 0,	
			.dev = 1,	
			.func = 1,	
			.vendor = 0x8086,	
			.device = 0x7010,	
			.classcode = 1,	
			.subclass = 1,	
			.progIF = 0x80,	
			.header = 0,	
			.data = {},	
			.index = 0,	
			.drvdata = 0
		},
		{
			.bus = 0,	
			.dev = 1,	
			.func = 3,	
			.vendor = 0x8086,	
			.device = 0x7113,	
			.classcode = 6,	
			.subclass = 0x80,	
			.progIF = 0,	
			.header = 0,	
			.data = {},	
			.index = 0,	
			.drvdata = 0
		},
		{
			.bus = 0,	
			.dev = 2,	
			.func = 0,	
			.vendor = 0x1234,	
			.device = 0x1111,	
			.classcode = 3,	
			.subclass = 0,	
			.progIF = 0,	
			.header = 0,	
			.data = {},	
			.index = 0,	
			.drvdata = 0
		},
		{
			.bus = 0,	
			.dev = 3,	
			.func = 0,	
			.vendor = 0x8086,	
			.device = 0x100e,	
			.classcode = 2,	
			.subclass = 0,	
			.progIF = 0,	
			.header = 0,	
			.data = {},	
			.index = 0,	
			.drvdata = 0
		}
	};

