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
	int failure = 0;

	for (; i < TEST_N_THDS; i++) {
		test_ts[i] = capmgr_thd_create(__test_thd_fn, (void *)i, &tid);
		assert(test_ts[i]);

		cos_thd_switch(test_ts[i]);
		if (thd_run_flag != i) {
			failure = 1;
			break;
		}
	}
	PRINTLOG(PRINT_DEBUG, "%s: thread capmgr unit tests\n", failure ? "FAILURE" : "SUCCESS");
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
test_heapmem(void)
{
	vaddr_t haddr;
	int failure = 0;

	haddr = memmgr_heap_page_allocn(TEST_N_HEAP_PAGES);
	if (!haddr || test_mem_readwrite(haddr, TEST_N_HEAP_PAGES)) failure = 1;
	PRINTLOG(PRINT_DEBUG, "%s: heap allocation capmgr unit tests\n", failure ? "FAILURE" : "SUCCESS");
}

#define TEST_PA 0xffff0000 /* test only arbitrary PA mapping and not r/w access to it */
#define TEST_PA_SZ (4*PAGE_SIZE)

static void
test_pa2va_map_only(void)
{
	vaddr_t va = memmgr_pa2va_map((paddr_t)TEST_PA, (unsigned int)TEST_PA_SZ);

	PRINTLOG(PRINT_DEBUG, "%s: phy->virt mapping capmgr unit tests\n", va == 0 ? "FAILURE" : "SUCCESS");
}

static void
test_sharedmem(void)
{
	cbuf_t id;
	vaddr_t addr;
	int failure = 0;

	id = memmgr_shared_page_allocn(TEST_N_SHMEM_PAGES, &addr);
	/* expect id == 1 to create a reader(shared memory map unit test */
	if (id != 1 || test_mem_readwrite(addr, TEST_N_SHMEM_PAGES)) failure = 1;
	PRINTLOG(PRINT_DEBUG, "%s: shared memory allocation capmgr unit tests\n", failure ? "FAILURE" : "SUCCESS");
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflag;

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	test_thds();
	test_sharedmem();
	test_heapmem();
	test_pa2va_map_only();
	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
