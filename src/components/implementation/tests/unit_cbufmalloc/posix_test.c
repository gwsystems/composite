#include <printc.h>
#include <cos_component.h>
#include <cbuf_mgr.h>
#include <assert.h>

#define NUM 1024
char *mptr[NUM];

void
cos_init(void *args)
{
	int err=0;

	int i, j;

	for (i = 0; i < NUM; i++) 
	{
		printc("Call to new malloc:\n");
		mptr[i] = (char *)malloc((i+1) * sizeof(int));

		assert(mptr[i]);
		
		printc("Call to new free:\n");
		for (j = 0; j < i; j++)
		{
			mptr[i][j] = j;
		}

        	for (j = 0; j < i; j++)
		{
			printc("%d,", mptr[i][j]);
		}

		printc("\n");
		free(mptr[i]);
	}

	return;
}
