/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <capmgr.h>
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
	int idx = 0, i, npages = 0;
	vaddr_t addr;

	npages = memmgr_shared_page_map(idx, &addr);
	PRINTC("Mapped shared @ %d:%lx, pages:%d\n", idx, addr, npages);

	assert(idx == 0); /* know that other comp created this before me. */
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
	spdid_t child;
	comp_flag_t childflag;

	PRINTC("Unit-test for capability manager shared memory interface\n");
	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	/* assuming this runs (initialization) after unit_capmgr component */
	test_shmem();
	PRINTC("Unit-test done.\n");
	hypercall_comp_init_done();

	SPIN();

	return;
}
