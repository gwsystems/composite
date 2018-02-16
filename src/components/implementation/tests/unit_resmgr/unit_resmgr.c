/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <resmgr.h>
#include <memmgr.h>
#include <llboot.h>

#define SPIN()            \
	do {              \
		while (1) \
			; \
	} while (0)

static cycles_t cycs_per_usec;

void
__test_thd_fn(void *d)
{
	printc("Thread created! %u\n", (int)d);
	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

#define TEST_N_THDS 5
thdcap_t test_ts[TEST_N_THDS] = { 0 };

static void
test_thds(void)
{
	int i = 0;

//	resmgr_initthd_create(0, 0);
	for (; i < TEST_N_THDS; i++) {
		printc("Creating thread @ %d\n", i);
		test_ts[i] = resmgr_thd_create(0, __test_thd_fn, (void *)i);
		assert(test_ts[i]);

		printc("Switching to thread %lu\n", test_ts[i]);
		cos_thd_switch(test_ts[i]);
	}
//	resmgr_thd_retrieve(0, 0, 0);
}

static void
test_asnd(void)
{
	resmgr_asnd_create(0, 0, 0);
}

static void
test_mem(void)
{
	memmgr_page_bump_alloc(0);

	/* memmgr_page_map(spdid, page_addr); */
	/* memmgr_page_map_at(spdid, page_addr, map_addr); -> for shared memory mapped at same addresses! */
}

void
cos_init(void)
{

	int                     id, ret;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	printc("Unit-test for Resource Manager interface\n");

	test_thds();
//	test_asnd();
//	test_mem();
//
//	printc("Unit-test done.\n");
	llboot_comp_init_done();

	SPIN();

	return;
}
