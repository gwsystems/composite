/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <rte_eal.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>


extern struct rte_pci_bus rte_pci_bus;
void
cos_init(void)
{
	int num = 0;
	num = rte_str_to_size("666\n");
	printc("num :%d, %p\n",num, &rte_pci_bus);
	int argc = 5, ret = -1;

	/* single core */
	char arg1[] = "DEBUG", arg2[] = "-l", arg3[] = "0", arg4[] = "--no-shconf", arg5[] = "--no-huge";
	char *argv[] = {arg1, arg2, arg3, arg4, arg5};
	ret = rte_eal_init(argc, argv);
	printc("end of init dpdk:%d\n",ret);
	while (1)
	{
		/* code */
	}
}
