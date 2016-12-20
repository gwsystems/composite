#include <print.h>
#include <cos_component.h>
#include <test_malloc_comp.h>
#include <cbuf.h>

#define NUM 10
static char *mptr[NUM];
static int flag = 0;
static cbuf_t cb;
static void* mem;

int 
call(void)
{
	printc("about to call malloc a bunch of times from spdid %d\n", cos_spd_id());
	static int a = 1;
	static int b = 2;
	int i, j;
	
	if (flag == 0)
	{
		flag = 1;
		int err=0; /* have we found a bug. Won't do fancy bug codes yet. 1 yes, 0 everything is fine */

		assert(cos_spd_id() == 12);

		for (i = 0; i < NUM; i++) {
			mptr[i] = (char *)malloc((i+1) * sizeof(int));
			assert(mptr[i]);
			for (j = 0; j < i; j++) {
				mptr[i][j] = j;
				printc("%d ", mptr[i][j]);
			}

			printc("\n");
		}

		//mem = cbuf_alloc(1024, &cb);
		//printc("Allocated cbuf %d\n", cb);
	}
	else
	{
		/* feel free to delete this line - I realize it might NOT be 14 but right now it is */
		assert(cos_spd_id() == 14);
		printc("Welcome to spd 14\n");

		for (i = 0; i < NUM; i++) {
			for (j = 0; j < i; j++) {
				mptr[i][j]++;
				printc("%d ", mptr[i][j]);
			}
			printc("\n");

			/* might as well do this */
//			free(mptr[i]);
		}

		printc("part 2 of F\n");

		//printc("Try to cbuf2cbuf cbuf\n");
		//mem = cbuf2buf(cb, 1024);

		/* This has to happen eventually - let's try here */
		//printc("Try to free cbuf\n");
		//cbuf_free(29); // the fork of 18	
		
		printc("Trying to call cbuf alloc again -- first time\n");
		cbuf_t cb2;
		void *mem2 = cbuf_alloc_ext(20, &cb2, CBUF_EXACTSZ);
		//printc("Allocated cbuf %d\n", cb2);

		printc("Trying to call cbuf alloc again -- second time\n");
		mem = cbuf_alloc_ext(20, &cb2, CBUF_EXACTSZ);

		memcpy(mem, "abc_123!", 8);
		printc("mem: [%s]\n", mem);
	}

	return a++ + b++;
}
