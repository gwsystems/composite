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

static int
test_mem_readwrite(vaddr_t addr, unsigned int size)
{
	unsigned int i;

	for (i = 0; i < size; i++) {
		vaddr_t page = addr + i * PAGE_SIZE;
		const char *str = test_strs[i % TEST_STR_NUM];

		memset((void *)page, 0, TEST_STR_MAX_LEN + 1);
		strcpy((char *)page, str);

		if (strcmp((char *)page, str) != 0) return 1;
	}

	return 0;
}

static void
test_shmem(void)
{
	cbuf_t id = 1;
	u32_t npages = 0;
	vaddr_t addr;
	int failure = 0;

	npages = memmgr_shared_page_map(id, &addr);
	/* know that other comp created this before me. */
	if (id != 1 || npages == 0 || test_mem_readwrite(addr, TEST_N_SHMEM_PAGES)) failure = 1;
	PRINTLOG(PRINT_DEBUG, "%s: shared memory map capmgr unit test\n", failure ? "FAILURE" : "SUCCESS");
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflag;

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	/* assuming this runs (initialization) after unit_capmgr component */
	test_shmem();
	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
