#include "consts.h"
#include <cos_types.h>
#include <cos_component.h>
#include <llprint.h>
#include <memmgr.h>

/* Size of the memory to be allocated in MB. */
#define TEST_SIZE 3072

void
cos_init(void)
{
	unsigned long npages = ((unsigned long)TEST_SIZE * 1024 * 1024 ) / PAGE_SIZE;
	printc("Test start... %ld pages (%d MB) to be allocated.\n", npages, TEST_SIZE);
	int t = 0;
	for (int i = 0; i < npages; i++) {
		vaddr_t test = memmgr_heap_page_allocn(1);
		memset((void *)test, 0, PAGE_SIZE);
		if (t != (i+1)*PAGE_SIZE/(1024*1024*1024)) {
			t = (i+1)*PAGE_SIZE/(1024*1024*1024);
			printc("\t==>Allocated %d GB of memory\n", t);
		}
	}
	printc("Success: Test done, allocate and memset : %ld pages\n", npages);

	while (1) ;
}
