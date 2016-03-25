#include "cos_init.h"
#include "rump_cos_alloc.h"

#include <cos_kernel_api.h>

extern struct cos_compinfo booter_info;

/* RG: The amount of memory to give RK to start */
#define RK_MEM ((1 << 30) + (1 << 5))
static unsigned long
alloc_initmem_all(void)
{
 	/* RG:
	 * 1 increment is 1 page, we start at 1 as the first page is fetched
	 * from cos_run.
	 */
	int count = 1;
	int max_rk = (RK_MEM / 4096);

	void *curpage;
	void *nxtpage = cos_page_bump_alloc(&booter_info);
	int  *nxtpage_test = (int *)nxtpage;
	*nxtpage_test = 1;

	while(count <= max_rk && nxtpage != NULL) {
		curpage = nxtpage;
		int *curpage_test = (int *)nxtpage;
		*curpage_test = 1;

		nxtpage = cos_page_bump_alloc(&booter_info);
		count++;
	}

	unsigned long max = (unsigned long)curpage;
	printc("max: %x\n", max);
	return max;
}

void
cos_run(char *cmdline)
{
	printc("Rump Kernel bootstrap on platform Composite\n");
	bmk_sched_init();
	printc("bmk_sched_init done\n");
	bmk_memalloc_init();
	printc("bmk_memalloc_init done\n");

	// Before bmk_pgalloc_loadmem is called, I need to alloc memory till we have enough or till failure
	// the start and end locations in memory to bmk_pgalloc
	//
	// Change the alloc method to cos_kern_page
	void* minptr = cos_page_bump_alloc(&booter_info);
	int *mintest = (int *)minptr;
	*mintest = 1;

	unsigned long min = (unsigned long)minptr;

	printc("first page: %x\n", min);
	unsigned long max = alloc_initmem_all();

	// bmk_pgalloc_loadmem is needed to get the memory area from Composite
	bmk_pgalloc_loadmem(min, max);
	printc("returned from bmk_pgalloc_loadmem\n");

	bmk_sched_startmain(bmk_mainthread, cmdline);
}
