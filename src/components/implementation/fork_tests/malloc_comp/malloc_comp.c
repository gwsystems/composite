#include <print.h>
#include <cos_component.h>
#include <test_malloc_comp.h>

#define NUM 10
static char *mptr[NUM];
static int flag = 0;

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
			printc("in O, %x\n", mptr[i]);

			for (j = 0; j < i; j++) {
				mptr[i][j] = j;
				printc("[%d] ", mptr[i][j]);
			}

			printc("\n");
		}
	}
	else
	{
		/* feel free to delete this line - I realize it might NOT be 14 but right now it is */
		assert(cos_spd_id() == 14);
		printc("Welcome to spd 14\n");

		for (i = 0; i < NUM; i++) {
			printc("in F, %x\n", mptr[i]);
			for (j = 0; j < i; j++) {
//				mptr[i][j]++;
//				printc("[%d] ", mptr[i][j]);
			}
			printc("\n");

	//		/* might as well do this */
	//		free(mptr[i]);
		}
	}

	return a++ + b++;
}
