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

#define TEST_N_THDS 5
static thdcap_t test_ts[TEST_N_THDS] = { 0 };
static int thd_run_flag = 0;

static void
__test_thd_fn(void *d)
{
	thd_run_flag = (int)d;
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

static void
test_thds(void)
{
	int i = 0;
	thdid_t tid;

	for (; i < TEST_N_THDS; i++) {
		test_ts[i] = capmgr_thd_create(__test_thd_fn, (void *)i, &tid);
		assert(test_ts[i]);

		cos_thd_switch(test_ts[i]);
		assert(thd_run_flag == i);
	}
	PRINTC("Creation/switch %d threads done.\n", TEST_N_THDS);
}

#define TEST_N_HEAP_PAGES 2048
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
test_mem(void)
{
	int idx;
	int i;
	vaddr_t addr, haddr;

	haddr = memmgr_heap_page_allocn(TEST_N_HEAP_PAGES);
	PRINTC("Alloc'd heap @ %lx, pages:%d\n", haddr, TEST_N_HEAP_PAGES);
	for (i = 0; i < TEST_N_HEAP_PAGES; i++) {
		vaddr_t page = haddr + i * PAGE_SIZE;
		const char *str = test_strs[i % TEST_STR_NUM];

		memset((void *)page, 0, TEST_STR_MAX_LEN + 1);
		strcpy((char *)page, str);

		assert(strcmp((char *)page, str) == 0);
	}
	PRINTC("Read/write %d pages done\n", TEST_N_HEAP_PAGES);

	idx = memmgr_shared_page_allocn(TEST_N_SHMEM_PAGES, &addr);
	PRINTC("Alloc'd shared @ %d:%lx, pages:%d\n", idx, addr, TEST_N_SHMEM_PAGES);

	assert(idx == 0); /* to create a reader and test */
	for (i = 0; i < TEST_N_SHMEM_PAGES; i++) {
		vaddr_t page = addr + i * PAGE_SIZE;
		const char *str = test_strs[i % TEST_STR_NUM];

		memset((void *)page, 0, TEST_STR_MAX_LEN + 1);
		strcpy((char *)page, str);

		assert(strcmp((char *)page, str) == 0);
	}
	PRINTC("Read/write %d pages done\n", TEST_N_SHMEM_PAGES);
}

void
cos_init(void)
{
	u64_t childbits;

	PRINTC("Unit-test for capability manager\n");
	hypercall_comp_children_get(cos_spd_id(), &childbits);
	assert(!childbits);

	test_thds();
	test_mem();
	PRINTC("Unit-test done.\n");
	hypercall_comp_init_done();

	SPIN();

	return;
}
