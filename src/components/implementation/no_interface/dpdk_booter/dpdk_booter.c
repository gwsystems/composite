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
	int argc = 8, ret = -1;

	/* single core, the first arg is program name */
	char arg1[] = "COS_DPDK_BOOTER", arg2[] = "-l", arg3[] = "0", arg4[] = "--no-shconf", arg5[] = "--no-huge", arg6[] = "--iova-mode=pa";
	/* log level can be changed to *debug* if needed, this will print lots of information */
	char arg7[] = "--log-level", arg8[] = "*:info";
	char *argv[] = {arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8};
	ret = cos_dpdk_init(argc, argv);
	printc("end of init dpdk:%d\n",ret);

}
