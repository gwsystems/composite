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
	
	if (flag == 0)
	{
		flag = 1;
		int err=0; /* have we found a bug. Won't do fancy bug codes yet. 1 yes, 0 everything is fine */
		int i, j;

		assert(cos_spd_id() == 12);

		for (i = 0; i < NUM; i++) {
			mptr[i] = (char *)malloc((i+1) * sizeof(int));
			assert(mptr[i]);

			for (j = 0; j < i; j++) {
				mptr[i][j] = j;
			}

			for (j = 0; j < i; j++) {
				printc("[%d] ", mptr[i][j]);
			}
			printc("\n");
			
			//free(mptr[i]);
		}
	}
	else
	{
		int i, j;
		for (i = 0; i < NUM; i++) {
			for (j = 0; j < i; j++) {
				mptr[i][j]++;
			}
			for (j = 0; j < i; j++) {
				printc("[%d] ", mptr[i][j]);
			}
			printc("\n");
		}
	}

	return a++ + b++;
}
