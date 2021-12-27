#include <cos_types.h>
#include <memmgr.h>

int
main(void)
{
	void *ptr;
	int   i;
	int   failure = 0;

	for (i = 12; i < 22; i++) {
		ptr = (void *) memmgr_heap_page_allocn_aligned(3, 1 << i);
		if ((unsigned long) ptr % 1 << i != 0) {
			failure = 1;
			break;
		}
		
	}

	PRINTLOG(PRINT_DEBUG, "%s: Aligned memory allocation is properly aligned\n", (failure) ? "FAILURE" : "SUCCESS");

	return 0;
}
