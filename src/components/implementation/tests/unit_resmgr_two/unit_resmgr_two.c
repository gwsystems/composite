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
#include <hypercall.h>

#define SPIN()            \
	do {              \
		while (1) \
			; \
	} while (0)

static cycles_t cycs_per_usec;

#define TEST_N_SHMEM_PAGES 64
#define TEST_STR_MAX_LEN 32
#define TEST_STR_NUM 5

char *test_strs[TEST_STR_NUM] = {
					"Hello",
					"Welcome",
					"Hi",
					"Howdy",
					"Goodbye",
				};

static void
test_shmem(void)
{
	int idx, i, npages = 0;
	vaddr_t addr;

	idx = memmgr_shared_page_map(0, 0, &addr, &npages);
	PRINTC("Mapped shared @ %d:%lx, pages:%d\n", idx, addr, npages);
	assert(idx == 0); /* to create a reader and test */
	assert(addr == memmgr_shared_page_vaddr(0, idx));

	for (i = 0; i < TEST_N_SHMEM_PAGES; i++) {
		vaddr_t page = addr + i * PAGE_SIZE;
		const char *str = test_strs[i % TEST_STR_NUM];

		assert(strcmp((char *)page, str) == 0);
	}
	PRINTC("Read %d shared pages done\n", TEST_N_SHMEM_PAGES);
}

void
cos_init(void)
{
	u64_t childbits;

	PRINTC("Unit-test for Resource Manager shared memory interface\n");
	hypercall_comp_childspdids_get(cos_spd_id(), &childbits);
	assert(!childbits);

	/* assuming this runs (initialization) after unit_resmgr component */
	test_shmem();
	PRINTC("Unit-test done.\n");
	hypercall_comp_init_done();

	SPIN();

	return;
}
