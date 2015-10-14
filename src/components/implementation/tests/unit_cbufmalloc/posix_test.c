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

	for(i=0; i<NUM; i++) {
        printc("Call to malloc:\n");
		mptr[i] = (char *)malloc(i+1);
		assert(mptr[i]);
		for(j=0; j<i; j++) mptr[i][j] = '$';
		//free(mptr[i]);
	}

	//RUN_TEST(fnmatch);
	//RUN_TEST(malloc);
	//RUN_TEST(memstream);
	//RUN_TEST(qsort);
	//RUN_TEST(sscanf);
	//RUN_TEST(snprintf);
	//RUN_TEST(string);
	//RUN_TEST(strtod);
	//RUN_TEST(strtol);
	//RUN_TEST(wcstol);

	//RUN(b_malloc_sparse, 0);
	//RUN(b_malloc_bubble, 0);
	//RUN(b_malloc_tiny1, 0);
	//RUN(b_malloc_tiny2, 0);
	//RUN(b_malloc_big1, 0);
	//RUN(b_malloc_big2, 0);
/*
	RUN(b_string_strstr, "abcdefghijklmnopqrstuvwxyz");
	RUN(b_string_strstr, "azbycxdwevfugthsirjqkplomn");
	RUN(b_string_strstr, "aaaaaaaaaaaaaacccccccccccc");
	RUN(b_string_strstr, "aaaaaaaaaaaaaaaaaaaaaaaaac");
	RUN(b_string_strstr, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac");
	RUN(b_string_memset, 0);
	RUN(b_string_strchr, 0);
	RUN(b_string_strlen, 0);

	RUN(b_regex_compile, "(a|b|c)*d*b");
	RUN(b_regex_search, "(a|b|c)*d*b");
	RUN(b_regex_search, "a{25}b");
*/

	return !!err;
}
