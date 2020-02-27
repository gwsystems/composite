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
#include <channel.h>
#include <hypercall.h>

static cycles_t cycs_per_usec;

#define TEST_N_THDS 5
static thdcap_t test_ts[NUM_CPU][TEST_N_THDS];

static void
__test_thd_fn(void *d)
{
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
}

static void
test_thds(void)
{
	int i = 0;
	thdid_t tid;
	int failure = 0;

	for (; i < TEST_N_THDS; i++) {
		test_ts[cos_cpuid()][i] = capmgr_thd_create(__test_thd_fn, (void *)i, &tid);
		assert(test_ts[cos_cpuid()][i]);

		if (cos_thd_switch(test_ts[cos_cpuid()][i])) {
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

#define SHMCHANNEL_KEY 0xff

static void
test_shmem_channel(void)
{
	cbuf_t id;
	vaddr_t addr;
	int failure = 0;

	id = channel_shared_page_allocn(SHMCHANNEL_KEY, TEST_N_SHMEM_PAGES, &addr);
	/* testing after test_sharedmem() */
	if (id <= 1 || test_mem_readwrite(addr, TEST_N_SHMEM_PAGES)) failure = 1;
	PRINTLOG(PRINT_DEBUG, "%s: shared memory channel allocation capmgr unit tests\n", failure ? "FAILURE" : "SUCCESS");
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflag;

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	test_thds();
	if (cos_cpuid() == INIT_CORE) {
		test_sharedmem();
		test_shmem_channel();
	}
	test_heapmem();
	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
