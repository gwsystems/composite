#include <printc.h>
#include <cos_component.h>
#include <cbuf_mgr.h>
#include <assert.h>
#include <test_malloc_comp.h>

#define NUM 10
char *mptr[NUM];

int
call(void)
{
	/* this message so we know we're in the component. That's right - I can't be sure. */
	printc("about to call malloc a bunch of times\n");

	int err=0; /* have we found a bug. Won't do fancy bug codes yet. 1 yes, 0 everything is fine */
	int i, j;

	for (i = 0; i < NUM; i++) {
		mptr[i] = (char *)malloc((i+1) * sizeof(int));
		assert(mptr[i]);

		for (j = 0; j < i; j++) {
			mptr[i][j] = j;
		}

		for (j = 0; j < i; j++) {
			printc("%d ", mptr[i][j]);
		}
		printc("\n");
		
		free(mptr[i]);
	}

	// this is never actually set to anything. Remove or use
	return err;
}
