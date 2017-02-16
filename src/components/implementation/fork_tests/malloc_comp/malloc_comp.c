#include <print.h>
#include <cos_component.h>
#include <test_malloc_comp.h>
#include <cbuf.h>
#include <cbuf_mgr.h>
#include <voter.h>

#define NUM 10
static char *mptr[NUM];
static int flag = 0;
static int confirm_flag = 0;
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

		//assert(cos_spd_id() == 12);

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
			free(mptr[i]);
		}
	}

done:
	return a++ + b++;
}

void cos_init(void) {
	if (!confirm_flag) {
		printc("malloc_comp_init\n");
		confirm_flag = 1;
		call();

		confirm(cos_spd_id());
		
		// to ignore fork for now
		printc("calling ping\n");
		int pingc = ping(cos_spd_id(), 0);
		printc("ping returned %d\n", pingc);
	} else {
		printc("calling ping\n");
		int pingc = ping(cos_spd_id(), 0);
		printc("ping returned %d\n", pingc);
	}
}
