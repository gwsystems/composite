#include <cos_types.h>
#include <memmgr.h>


static void
test_alignment()
{
	void *ptr;
	int   i;
	int   failure = 0;

	for (i = 12; i < 22; i++) {
		ptr = (void *)memmgr_heap_page_allocn_aligned(3, 1 << i);
		if ((unsigned long)ptr % 1 << i != 0) {
			failure = 1;
			break;
		}
		
	}

	printc("%s: Aligned memory allocation is properly aligned\n", (failure) ? "FAILURE" : "SUCCESS");
}

static void
test_aligned_allocation_continuity()
{
	int   i;
	int   n = 5;
	char *ptr;

	/*
	 * making sure we dont fault between pages 
	 *(that pages are still contiguous with the alignment)
	 */
	for (i = 12; i < 22; i++) {
		ptr = (char *)memmgr_heap_page_allocn_aligned(n, 1 << i);
		for (i = 0; i < n*4096; i++) {
			ptr[i] = '\1';
		}
	}	

	printc("SUCCESS: Aligned memory allocation is continguous across pages\n");

}

int
main(void)
{
	test_alignment();
	test_aligned_allocation_continuity();
	return 0;
}
