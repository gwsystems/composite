/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <rte_eal.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <cos_dpdk_adapter.h>

void
cos_init(void)
{
	int num = 0;
	int argc = 6, ret = -1;

	/* single core */
	char arg1[] = "DEBUG", arg2[] = "-l", arg3[] = "0", arg4[] = "--no-shconf", arg5[] = "--no-huge", arg6[] = "--iova-mode=pa";
	char *argv[] = {arg1, arg2, arg3, arg4, arg5, arg6};
	ret = cos_dpdk_init(argc, argv);
	printc("end of init dpdk:%d\n",ret);

}
