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
		printc("1: start\n");
		mptr[i] = (char *)malloc((i+1) * sizeof(int));
		printc("2\n");
		assert(mptr[i]);
		printc("3\n");

		for (j = 0; j < i; j++) {
			printc("4\n");
			mptr[i][j] = j;
			printc("5\n");
		}

		printc("6\n");

		printc("Final status: ");
		for (j = 0; j < i; j++) {
			printc("%d ", mptr[i][j]);
		}

		printc("\n6.5: freeing\n");
		free(mptr[i]);
		printc("7\n");
	}
	
	for (i = 0; i < NUM; i++) {
		mptr[i] = (char *)malloc((i+1) * sizeof(int));

		assert(mptr[i]);

		for (j = 0; j < i; j++) {
			mptr[i][j] = j;
		}
		
		mptr[i] = realloc(mptr[i], (i + 1 + i) * sizeof(int));

		free(mptr[i]);
	}

	return err;
}
