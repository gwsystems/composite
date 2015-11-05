#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <print.h>
#include <cos_component.h>

#define NUM 1024
char *mptr[NUM];

int
test_malloc(void)
{
	int i, j;

	for(i=0; i<NUM; i++) {
		mptr[i] = (char *)malloc(i+1);
		assert(mptr[i]);
		for(j=0; j<i; j++) mptr[i][j] = '$';
		free(mptr[i]);
	}
	
	for(i=0; i<NUM; i++) {
		mptr[i] = (char *)malloc(i+1);
		assert(mptr[i]);
		for(j=0; j<i; j++) mptr[i][j] = '$';
	}
	for(i=0; i<NUM; i++) {
		for(j=0; j<i; j++) assert(mptr[i][j] == '$');
		free(mptr[i]);
	}

	for(i=0; i<NUM; i++) {
		mptr[i] = (char *)malloc(4096*(i+1));
		assert(mptr[i]);
		memset(mptr[i], '$', 4096*(i+1));
		free(mptr[i]);
	}

	for(i=0; i<125; i++) {
		mptr[i] = (char *)malloc(4096*(i+1));
		assert(mptr[i]);
		memset(mptr[i], '$', 4096*(i+1));
	}
	for(i=0; i<125; i++) {
		for(j=0; j<NUM; j++) assert(mptr[i][j] == '$');
		free(mptr[i]);
	}

	return 0;
}
