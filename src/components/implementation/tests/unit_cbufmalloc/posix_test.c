#include <printc.h>
#include <cos_component.h>
#include <cbuf_mgr.h>
#include <assert.h>

#define NUM 10
char *mptr[NUM];

void
cos_init(void *args)
{
	int err=0;
	int i, j;

	for (i = 0; i < NUM; i++) {
		mptr[i] = (char *)malloc((i+1) * sizeof(int));

		assert(mptr[i]);
		
		for (j = 0; j < i; j++) {
			mptr[i][j] = j;
		}

		free(mptr[i]);
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

	char* p = malloc(10);
	assert(p);

	printc("reallocating p to 0\n");
	realloc(p, 0);

	return;
}
